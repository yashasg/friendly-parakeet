// Reads vcpkg.json and submits a dependency snapshot to GitHub's
// Dependency Submission API so dependency-review-action can work.
//
// Usage (in GitHub Actions):
//   node .github/scripts/vcpkg-dependency-snapshot.js

const fs = require("fs");
const path = require("path");

const manifest = JSON.parse(
  fs.readFileSync(path.join(process.env.GITHUB_WORKSPACE || ".", "vcpkg.json"), "utf8")
);

function parseDep(dep) {
  if (typeof dep === "string") return { name: dep };
  return { name: dep.name, platform: dep.platform || null };
}

const deps = (manifest.dependencies || []).map(parseDep);

const snapshot = {
  version: 0,
  sha: process.env.GITHUB_SHA,
  ref: process.env.GITHUB_REF,
  job: {
    id: process.env.GITHUB_RUN_ID || "local",
    correlator: `${process.env.GITHUB_WORKFLOW || "dep-submit"}_${process.env.GITHUB_JOB || "submit"}`,
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
            package_url: `pkg:vcpkg/${d.name}`,
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
console.log(`  ${deps.length} dependencies: ${deps.map((d) => d.name).join(", ")}`);
