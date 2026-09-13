# Preparing a public release

The source repository is https://github.com/taklab-org/INTLAB-rounding-fix. Build and launch scripts never push commits or create GitHub releases.

Before publishing:

1. Preserve the MIT license and third-party notices; review licensing and attribution for any newly added code.
2. Build from a clean checkout on the intended Apple Silicon environment. Run native tests, the MATLAB check and optional INTLAB tests. Record which tests were actually run.
3. Review `git status` and the staged diff. Build products, generated local configuration and raw logs belong only under ignored `build/`; do not include MATLAB/INTLAB installations, headers, caches or personal startup settings.
4. Commit the reviewed source using the owner's intended public Git identity. Use the existing `taklab-org/INTLAB-rounding-fix` repository and push the reviewed commit without overwriting unrelated remote work.
5. Tag the reviewed commit as `v0.1.0` and create a prerelease describing the non-public API dependency and tested environment. Update the changelog when it is actually released.

Source distribution is the initial policy; users compile the dylib and MEX against their installed tools. No prebuilt binary is necessary for the first release. Avoid claims of general correctness based only on startup flags or scalar tests.

Relevant GitHub instructions:

- https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository
- https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
