#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: convert.sh <input-xml-dir> <output-c-dir>" >&2
  exit 1
fi

INPUT_DIR="$1"
OUTPUT_DIR="$2"
LVGL_CLI_BIN="${LVGL_CLI:-}"
LVGL_CLI_NODE="${LVGL_CLI_NODE:-0}"
LVGL_NODE_BIN="${LVGL_NODE:-node}"
LVGL_RESOURCES_ZIP="${LVGL_RESOURCES_ZIP:-}"

# CMake -E env may pass quoted values; normalize one wrapping quote pair.
LVGL_CLI_BIN="${LVGL_CLI_BIN#\"}"
LVGL_CLI_BIN="${LVGL_CLI_BIN%\"}"
LVGL_CLI_NODE="${LVGL_CLI_NODE#\"}"
LVGL_CLI_NODE="${LVGL_CLI_NODE%\"}"
LVGL_NODE_BIN="${LVGL_NODE_BIN#\"}"
LVGL_NODE_BIN="${LVGL_NODE_BIN%\"}"
LVGL_RESOURCES_ZIP="${LVGL_RESOURCES_ZIP#\"}"
LVGL_RESOURCES_ZIP="${LVGL_RESOURCES_ZIP%\"}"

if [[ -z "${LVGL_CLI_BIN}" ]]; then
  echo "LVGL_CLI is not set; cannot run LVGL XML conversion." >&2
  exit 1
fi

if [[ "${LVGL_CLI_NODE}" == "1" ]] && ! command -v "${LVGL_NODE_BIN}" >/dev/null 2>&1; then
  echo "LVGL_NODE='${LVGL_NODE_BIN}' is not executable; cannot run JS LVGL CLI." >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

shopt -s nullglob
xml_files=("${INPUT_DIR}"/*.xml)
if [[ ${#xml_files[@]} -eq 0 ]]; then
  echo "No LVGL XML files found in ${INPUT_DIR}."
  exit 0
fi

if [[ "${LVGL_CLI_NODE}" == "1" ]] && [[ "$(basename "${LVGL_CLI_BIN}")" == "lved-cli.js" ]]; then
  lved_project_dir="${OUTPUT_DIR}/.lved_project"
  lved_home_dir="${OUTPUT_DIR}/.lved_home"
  lved_cache_dir="${lved_home_dir}/Library/Application Support/lvgl-editor"
  rm -rf "${lved_project_dir}"
  mkdir -p "${lved_project_dir}/content/ui/screens" "${lved_project_dir}/browser"
  cp "${xml_files[@]}" "${lved_project_dir}/content/ui/screens/"
  printf '%s\n' '/* lved stub */' > "${lved_project_dir}/browser/default-stylesheet.css"

  if [[ -z "${LVGL_RESOURCES_ZIP}" ]]; then
    LVGL_RESOURCES_ZIP="$(cd "$(dirname "${LVGL_CLI_BIN}")" && pwd)/lvgl-resources.zip"
  fi
  if [[ ! -f "${LVGL_RESOURCES_ZIP}" ]]; then
    echo "LVGL_RESOURCES_ZIP='${LVGL_RESOURCES_ZIP}' is missing; cannot scaffold lved project templates." >&2
    exit 1
  fi

  rm -rf "${lved_home_dir}"
  mkdir -p "${lved_cache_dir}"
  unzip -q -o "${LVGL_RESOURCES_ZIP}" -d "${lved_cache_dir}"

  template_dir="${lved_cache_dir}/resources/templates/new-project-template"
  cp "${template_dir}/project.xml" "${lved_project_dir}/project.xml"
  cp "${template_dir}/globals.xml" "${lved_project_dir}/globals.xml"

  echo "LVGL (lved): generating project from ${lved_project_dir}"
  lved_node_localstorage="${lved_home_dir}/.node-localstorage"
  HOME="${lved_home_dir}" \
    "${LVGL_NODE_BIN}" "--localstorage-file=${lved_node_localstorage}" "${LVGL_CLI_BIN}" \
    generate "${lved_project_dir}" --toolchain-mode none

  shopt -s nullglob
  generated_files=("${lved_project_dir}"/*.c "${lved_project_dir}"/*.h)
  if [[ ${#generated_files[@]} -eq 0 ]]; then
    echo "lved-cli.js finished but produced no C/H files in ${lved_project_dir}." >&2
    exit 1
  fi
  cp "${generated_files[@]}" "${OUTPUT_DIR}/"
  exit 0
fi

for xml in "${xml_files[@]}"; do
  base="$(basename "${xml}" .xml)"
  out="${OUTPUT_DIR}/${base}.c"
  echo "LVGL: ${xml} -> ${out}"
  if [[ "${LVGL_CLI_NODE}" == "1" ]]; then
    "${LVGL_NODE_BIN}" "${LVGL_CLI_BIN}" convert --input "${xml}" --output "${out}"
  else
    "${LVGL_CLI_BIN}" convert --input "${xml}" --output "${out}"
  fi
done
