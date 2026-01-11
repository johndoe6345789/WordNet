# GitHub Actions Workflows

This directory contains GitHub Actions workflows for continuous integration and continuous release of WordNet.

## Workflows

### 1. CI Workflow (`ci.yml`)

**Triggers:**
- Push to `main`, `master`, or `develop` branches
- Pull requests to `main`, `master`, or `develop` branches
- Manual workflow dispatch

**What it does:**
- Builds WordNet on Linux (Ubuntu), macOS, and Windows
- Runs basic tests on each platform
- Uploads build artifacts for inspection

**Platform-specific details:**
- **Linux**: Uses apt-get to install CMake, Ninja, GCC, and Tcl/Tk
- **macOS**: Uses Homebrew to install dependencies
- **Windows**: Uses Chocolatey to install CMake and Ninja

### 2. Release Workflow (`release.yml`)

**Triggers:**
- Push of tags matching `v*.*.*` or `v*.*` (e.g., `v3.0.0`, `v3.0.1`)
- Manual workflow dispatch with version input

**What it does:**
1. Creates a GitHub Release with auto-generated release notes
2. Builds WordNet for Linux, macOS, and Windows
3. Creates platform-specific archives:
   - Linux: `wordnet-vX.X.X-linux-x64.tar.gz`
   - macOS: `wordnet-vX.X.X-macos-x64.tar.gz`
   - Windows: `wordnet-vX.X.X-windows-x64.zip`
4. Uploads all archives as release assets

**To create a release:**
```bash
# Create and push a tag
git tag -a v3.0.1 -m "Release v3.0.1"
git push origin v3.0.1
```

Or use the manual workflow dispatch in the GitHub Actions UI.

### 3. Continuous Release Workflow (`continuous-release.yml`)

**Triggers:**
- Weekly schedule (Monday at 00:00 UTC)
- Manual workflow dispatch

**What it does:**
1. Checks if there are new commits since the last release
2. If changes exist, builds and tests the code
3. Automatically creates a new version tag (increments patch version)
4. Pushes the tag, which triggers the Release workflow

**Versioning:**
- Automatically increments the patch version (e.g., v3.0.0 → v3.0.1)
- First release defaults to v3.0.0 if no tags exist

## Usage Examples

### Running CI Manually

1. Go to the "Actions" tab in your GitHub repository
2. Select "CI" workflow
3. Click "Run workflow" button
4. Select the branch and run

### Creating a Manual Release

1. Go to the "Actions" tab in your GitHub repository
2. Select "Release" workflow
3. Click "Run workflow" button
4. Enter the version (e.g., `v3.0.1`)
5. Click "Run workflow"

### Triggering Continuous Release

1. Go to the "Actions" tab in your GitHub repository
2. Select "Continuous Release" workflow
3. Click "Run workflow" button
4. Check "Create a release" option
5. Click "Run workflow"

## Release Process

The recommended release process:

1. **Development**: Work on features/fixes on feature branches
2. **CI Testing**: CI runs automatically on PRs and commits
3. **Merge**: Merge approved changes to main/master
4. **Release**: Either:
   - Wait for weekly continuous release (automated)
   - Manually create and push a version tag
   - Use workflow dispatch to create a release

## Configuration

### Modifying Build Matrix

Edit the `matrix` section in `ci.yml` or `release.yml` to:
- Add/remove operating systems
- Add different build types (Debug, RelWithDebInfo)
- Test different CMake configurations

### Changing Release Schedule

Modify the `cron` expression in `continuous-release.yml`:
- Daily: `'0 0 * * *'`
- Weekly on Friday: `'0 0 * * 5'`
- Monthly: `'0 0 1 * *'`

### Customizing Versioning

The continuous release workflow uses semantic versioning with auto-increment of patch version. To customize:
1. Edit the version calculation logic in `check_commits` step
2. Modify the tag format if needed

## Troubleshooting

### Build failures

- Check the build logs in the Actions tab
- Verify all dependencies are correctly installed
- Ensure CMakeLists.txt is compatible with all platforms

### Release not created

- Verify the tag format matches `v*.*.*`
- Check that GitHub Actions has write permissions
- Ensure GITHUB_TOKEN has appropriate permissions

### Artifacts not uploading

- Check artifact name doesn't contain special characters
- Verify the file paths are correct
- Ensure files exist after build step

## Security

- Workflows use `GITHUB_TOKEN` which is automatically provided by GitHub
- No external secrets are required
- All dependencies are installed from official sources (apt, brew, choco)

## Contributing

When modifying workflows:
1. Test changes on a fork first
2. Use `workflow_dispatch` to test manually
3. Verify all platforms build successfully
4. Update this README if adding/modifying workflows
