NPROC := $(shell nproc 2>/dev/null || echo 4)
VERSION_FILE := VERSION
RELEASE_DIR := build/release

.DEFAULT_GOAL := linux
.PHONY: all linux windows-cross windows-msys clean format help release release-precheck

help:
	@echo "Targets:"
	@echo "  linux          native Linux build (default)"
	@echo "  windows-cross  cross-compiled from Linux via MinGW-w64, tested under Wine"
	@echo "  windows-msys   native Windows build, run from the MSYS2 UCRT64 shell"
	@echo "  format         clang-format the whole source tree"
	@echo "  clean          remove the build directory"
	@echo "  release        build, version-tag, and push a release (linux + windows-cross)"

format:
	find . -type f \( -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i {} +

linux: format
	cmake -S . -B build/linux
	cmake --build build/linux -j$(NPROC)
	ctest --test-dir build/linux --output-on-failure

windows-cross: format
	cmake -S . -B build/windows-cross -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
	cmake --build build/windows-cross -j$(NPROC)
	ctest --test-dir build/windows-cross --output-on-failure

# Builds ts-cli natively for 64-bit Windows using the MSYS2 UCRT64
# environment and runs the test suite directly on Windows. Intentionally
# separate from windows-cross, which cross-compiles from Linux via
# cmake/toolchains/mingw-w64.cmake and runs its tests through Wine.
windows-msys: format
	@if [ "$$MSYSTEM" != "UCRT64" ]; then \
		echo "error: run 'make windows-msys' from the MSYS2 UCRT64 shell (MSYSTEM=$$MSYSTEM)" >&2; \
		exit 1; \
	fi
	@if [ "$$MINGW_PREFIX" != "/ucrt64" ]; then \
		echo "error: expected MINGW_PREFIX=/ucrt64, got $$MINGW_PREFIX" >&2; \
		exit 1; \
	fi
	@for cmd in cmake ninja pkg-config gcc g++ cygpath; do \
		command -v "$$cmd" >/dev/null 2>&1 || { echo "error: required command not found: $$cmd (see BUILDING.md)" >&2; exit 1; }; \
	done
	@for module in libsodium opus openssl; do \
		pkg-config --exists "$$module" || { echo "error: pkg-config module not found: $$module (see BUILDING.md)" >&2; exit 1; }; \
	done
	env -u CMAKE_TOOLCHAIN_FILE -u PKG_CONFIG_SYSROOT_DIR -u PKG_CONFIG_LIBDIR -u PKG_CONFIG_PATH \
		cmake -S . -B build/windows-msys -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DOPENSSL_ROOT_DIR="$$(cygpath -m "$$MINGW_PREFIX")" \
		-DOPENSSL_USE_STATIC_LIBS=TRUE
	cmake --build build/windows-msys
	ctest --test-dir build/windows-msys --output-on-failure

clean:
	rm -rf build

release-precheck:
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "error: working tree is not clean; commit or stash changes before releasing" >&2; \
		exit 1; \
	fi
	@branch="$$(git branch --show-current)"; \
	git fetch origin "$$branch" >/dev/null 2>&1 || { \
		echo "error: 'git fetch origin $$branch' failed -- check your network/remote before releasing" >&2; \
		exit 1; \
	}; \
	if [ -n "$$(git log --oneline "$$branch"..origin/"$$branch" 2>/dev/null)" ]; then \
		echo "error: local $$branch is behind origin/$$branch -- pull/rebase first, then retry" >&2; \
		exit 1; \
	fi

# Builds linux + windows-cross release binaries, then interactively prompts
# for the new version (never bumps automatically), tags, and pushes the
# commit and tag to origin with plain git -- no GitHub-specific tooling
# involved. The packaged binaries are left in build/release/ for a
# maintainer to attach when creating the release on GitHub (or anywhere
# else) by hand.
#
# set -e plus the trap below mean any failure after the confirmation prompt
# rolls back whatever this run did that never made it to origin (an
# unpushed local commit and/or tag). Once the branch push itself succeeds,
# that commit is shared history -- the trap will not touch it, only clean
# up a since-orphaned local tag, and tells you exactly what's left to do.
release: release-precheck linux windows-cross
	@set -e; \
	current_version="$$(cat $(VERSION_FILE) 2>/dev/null || echo 0.0.0)"; \
	echo "Current version: $$current_version"; \
	printf "New version (semantic, e.g. 0.2.0 or 0.2.0-beta): "; \
	read -r new_version; \
	if ! printf '%s' "$$new_version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$$'; then \
		echo "error: '$$new_version' is not a valid semantic version (expected X.Y.Z or X.Y.Z-prerelease)" >&2; \
		exit 1; \
	fi; \
	if [ "$$new_version" = "$$current_version" ]; then \
		echo "error: new version is identical to the current version ($$current_version)" >&2; \
		exit 1; \
	fi; \
	lowest="$$(printf '%s\n%s\n' "$$current_version" "$$new_version" | sort -V | head -n1)"; \
	if [ "$$lowest" = "$$new_version" ]; then \
		echo "warning: $$new_version does not look newer than $$current_version by semantic-version ordering"; \
	fi; \
	branch="$$(git branch --show-current)"; \
	mkdir -p $(RELEASE_DIR); \
	cp build/linux/client/ts-cli "$(RELEASE_DIR)/ts-cli-v$$new_version-linux-x86_64"; \
	cp build/windows-cross/client/ts-cli.exe "$(RELEASE_DIR)/ts-cli-v$$new_version-windows-x86_64.exe"; \
	echo; \
	echo "Release artifacts:"; \
	ls -l "$(RELEASE_DIR)/ts-cli-v$$new_version-linux-x86_64" "$(RELEASE_DIR)/ts-cli-v$$new_version-windows-x86_64.exe"; \
	echo; \
	echo "About to, on branch '$$branch':"; \
	echo "  1. write $$new_version to $(VERSION_FILE) and commit it"; \
	echo "  2. create annotated tag v$$new_version"; \
	echo "  3. push the commit and tag to origin"; \
	printf "Continue? [y/N] "; \
	read -r confirm; \
	case "$$confirm" in \
		y|Y) ;; \
		*) echo "Aborted -- nothing was committed, tagged, or pushed."; exit 1 ;; \
	esac; \
	start_commit="$$(git rev-parse HEAD)"; \
	committed=0; tagged_local=0; pushed_commit=0; \
	rollback() { \
		status=$$?; \
		[ "$$status" = "0" ] && return; \
		if [ "$$tagged_local" = "1" ]; then \
			git tag -d "v$$new_version" >/dev/null 2>&1 || true; \
		fi; \
		if [ "$$pushed_commit" = "1" ]; then \
			echo "warning: the version-bump commit was already pushed to origin/$$branch and was NOT rolled back (that is shared history now)." >&2; \
			echo "         Only the local tag (if any) was cleaned up. Fix forward: re-run 'make release', or tag/push manually." >&2; \
		elif [ "$$committed" = "1" ]; then \
			git reset --hard "$$start_commit" >/dev/null 2>&1 || true; \
			echo "Rolled back the local commit -- nothing was pushed. Working tree is back to $$start_commit." >&2; \
		fi; \
	}; \
	trap rollback EXIT; \
	echo "$$new_version" > $(VERSION_FILE); \
	git add $(VERSION_FILE); \
	git commit -m "Release v$$new_version"; \
	committed=1; \
	git tag -a "v$$new_version" -m "Release v$$new_version"; \
	tagged_local=1; \
	git push origin "$$branch"; \
	pushed_commit=1; \
	git push origin "v$$new_version"; \
	trap - EXIT; \
	echo; \
	echo "Tag v$$new_version pushed to origin. Create the release on GitHub from that tag and attach:"; \
	echo "  $(RELEASE_DIR)/ts-cli-v$$new_version-linux-x86_64"; \
	echo "  $(RELEASE_DIR)/ts-cli-v$$new_version-windows-x86_64.exe"
