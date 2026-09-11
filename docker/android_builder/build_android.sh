#!/usr/bin/env bash
set -euo pipefail

project_dir="${PROJECT_DIR:-/workspace/client}"
output_dir="${OUTPUT_DIR:-/workspace/output}"
build_dir="${BUILD_DIR:-/tmp/werewolf-android-build}"

if [[ ! -f "${project_dir}/WerewolfClient.pro" ]]; then
  echo "Qt project not found: ${project_dir}/WerewolfClient.pro" >&2
  exit 2
fi
if [[ "${build_dir}" != /tmp/werewolf-* ]]; then
  echo "BUILD_DIR must stay under /tmp/werewolf-*" >&2
  exit 2
fi

rm -rf -- "${build_dir}"
mkdir -p "${build_dir}" "${output_dir}"
cd "${build_dir}"

"${QT_ANDROID_PATH}/bin/qmake" "${project_dir}/WerewolfClient.pro" CONFIG+=release
make -j"$(nproc)"
make apk

unsigned_apk="$(find "${build_dir}" -type f -path '*/outputs/apk/*' -name '*.apk' | head -n 1)"
if [[ -z "${unsigned_apk}" ]]; then
  echo "APK was not generated" >&2
  exit 3
fi

keystore="${ANDROID_KEYSTORE:-/tmp/werewolf-debug.keystore}"
alias_name="${ANDROID_KEY_ALIAS:-androiddebugkey}"
store_password="${ANDROID_STORE_PASSWORD:-android}"
key_password="${ANDROID_KEY_PASSWORD:-android}"

if [[ ! -f "${keystore}" ]]; then
  keytool -genkeypair -noprompt \
    -keystore "${keystore}" -storepass "${store_password}" \
    -alias "${alias_name}" -keypass "${key_password}" \
    -dname "CN=WolfGame Debug,O=WolfGame,C=CN" \
    -keyalg RSA -keysize 2048 -validity 10000
fi

signed_apk="${output_dir}/WerewolfClient-arm64-v8a.apk"
apksigner sign --ks "${keystore}" --ks-key-alias "${alias_name}" \
  --ks-pass "pass:${store_password}" --key-pass "pass:${key_password}" \
  --out "${signed_apk}" "${unsigned_apk}"
apksigner verify --verbose "${signed_apk}"

echo "APK ready: ${signed_apk}"
