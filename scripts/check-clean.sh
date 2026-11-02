#!/usr/bin/env bash
set -u

SRC="TMessagesProj/src TMessagesProj/build.gradle TMessagesProj_AppStandalone settings.gradle build.gradle gradle.properties"
FAIL=0

GRADLE_SRC="settings.gradle build.gradle gradle.properties TMessagesProj/build.gradle TMessagesProj_AppStandalone/build.gradle"

scan() {
  grep -rIn --exclude-dir=build --exclude-dir=jni --exclude-dir=.git \
    -e "$1" $2 2>/dev/null | grep -vE ':[0-9]+:[[:space:]]*(\*|//|/\*)'
}

check() {
  local pattern="$1"
  local label="$2"
  local scope="${3:-$SRC}"
  local hits
  hits=$(scan "$pattern" "$scope" | cut -d: -f1 | sort -u | wc -l)
  if [ "$hits" -ne 0 ]; then
    echo "FAIL: $label -- $hits file(s)"
    scan "$pattern" "$scope" | cut -d: -f1 | sort -u | head -5 | sed 's/^/      /'
    FAIL=1
  else
    echo "ok:   $label"
  fi
}

check "unnarsx"                     "old package name"
check "[Cc]herrygram"               "old brand name"
check "adsgram"                     "Adsgram ads"
check "safe-stars"                  "safe-stars service"
check "crashlytics"                 "Firebase Crashlytics"
check "RemoteConfigHelper\|RemoteConfigConstants\|FirebaseRemoteConfig" "Firebase Remote Config"
check "arslan4k1390"                "upstream author endpoints"
check "ettacent\|nimarko"           "NimarkoGram infrastructure"
check "huawei\|agconnect"           "Huawei build wiring" "$GRADLE_SRC"

if [ "$FAIL" -ne 0 ]; then
  echo
  echo "check-clean: FAILED"
  exit 1
fi
echo
echo "check-clean: PASSED"
