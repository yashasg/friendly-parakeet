// Reads vcpkg.json and submits a dependency snapshot to GitHub's
// Dependency Submission API so dependency-review-action can work.
//
// Usage (in GitHub Actions):
//   node .github/scripts/vcpkg-dependency-snapshot.js
//
// Respects vcpkg platform expressions (e.g. "!linux", "osx", "windows") so
// that the submitted snapshot only contains dependencies applicable to the
// current runner OS.

const fs = require("fs");
const path = require("path");

const manifest = JSON.parse(
  fs.readFileSync(path.join(process.env.GITHUB_WORKSPACE || ".", "vcpkg.json"), "utf8")
);

// Map GitHub Actions RUNNER_OS to vcpkg platform identifiers.
function getVcpkgPlatform() {
  const runnerOs = (process.env.RUNNER_OS || "").toLowerCase();
  if (runnerOs === "linux") return "linux";
  if (runnerOs === "macos") return "osx";
  if (runnerOs === "windows") return "windows";
  return null; // unknown / running locally
}

// Evaluate a simple vcpkg platform expression against the current platform.
// Supports bare identifiers ("linux", "osx", "windows") and their negations
// ("!linux", etc.).  Returns true when the dependency should be included.
// Defaults to true (include) when expr is absent (unconditional dependency)
// or when platform is unknown (e.g. running locally without RUNNER_OS).
function evalPlatform(expr, platform) {
  if (!expr || !platform) return true;
  const trimmed = expr.trim();
  if (trimmed.startsWith("!")) {
    return platform !== trimmed.slice(1).trim();
  }
  return platform === trimmed;
}

function parseDep(dep) {
  if (typeof dep === "string") return { name: dep, platform: null };
  return { name: dep.name, platform: dep.platform || null };
}

const currentPlatform = getVcpkgPlatform();
const deps = (manifest.dependencies || [])
  .map(parseDep)
  .filter((d) => evalPlatform(d.platform, currentPlatform));

const snapshot = {
  version: 0,
  sha: process.env.GITHUB_SHA,
  ref: process.env.GITHUB_REF,
  job: {
    id: process.env.GITHUB_RUN_ID || "local",
    // Include the platform in the correlator so each OS produces its own
    // snapshot entry rather than overwriting a single shared one.
    correlator: `${process.env.GITHUB_WORKFLOW || "dep-submit"}_${process.env.GITHUB_JOB || "submit"}_${currentPlatform || "unknown"}`,
  },
  detector: {
    name: "vcpkg-manifest-parser",
    version: "1.0.0",
    url: "https://github.com/microsoft/vcpkg",
  },
  scanned: new Date().toISOString(),
  manifests: {
    "vcpkg.json": {
      name: manifest.name || "unknown",
      file: { source_location: "vcpkg.json" },
      resolved: Object.fromEntries(
        deps.map((d) => [
          d.name,
          {
            package_url: `pkg:generic/${d.name}?repository_url=https://github.com/microsoft/vcpkg`,
            relationship: "direct",
            scope: d.name === "catch2" ? "development" : "runtime",
          },
        ])
      ),
    },
  },
};

const output = JSON.stringify(snapshot, null, 2);
fs.writeFileSync("dependency-snapshot.json", output);
console.log("Dependency snapshot written to dependency-snapshot.json");
console.log(`  Platform: ${currentPlatform || "unknown"}`);
console.log(`  ${deps.length} dependencies: ${deps.map((d) => d.name).join(", ")}`);
