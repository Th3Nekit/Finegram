#!/usr/bin/env bash
# Checks for retired SDKs, branding and service endpoints.
set -u

SRC="TMessagesProj/src TMessagesProj/build.gradle TMessagesProj_AppStandalone settings.gradle build.gradle gradle.properties"
FAIL=0

GRADLE_SRC="settings.gradle build.gradle gradle.properties TMessagesProj/build.gradle TMessagesProj_AppStandalone/build.gradle"

# Copyright headers of Cherrygram must stay (GPL-2.0 attribution), so comment lines
# are not counted as findings.
filter_known_references() {
  local hit path content
  while IFS= read -r hit; do
    path=${hit%%:*}
    content=${hit#*:}
    content=${content#*:}
    content=${content%$'\r'}
    content="${content#"${content%%[![:space:]]*}"}"
    case "$path:$content" in
      'TMessagesProj/src/main/java/com/th3nekit/finegram/preferences/AboutPreferencesEntry.java:private static final String CHERRY_URL = "https://github.com/arsLan4k1390/Cherrygram";' | \
      'TMessagesProj/src/main/java/com/th3nekit/finegram/preferences/AboutPreferencesEntry.java:getString(R.string.FG_Credits_Cherry), "Cherrygram"));' | \
      'TMessagesProj/src/main/java/com/th3nekit/finegram/core/configs/FinegramCoreConfig.kt:var safeStarsUrl by sharedPreferences.string("FG_SafeStarsURL", "https://safe-stars.com/")' | \
      'TMessagesProj/src/main/java/com/th3nekit/finegram/core/configs/FinegramCoreConfig.kt:var safeStarsUrlRu by sharedPreferences.string("FG_SafeStarsURL_RU", "https://safe-stars.com/ru/")' | \
      'TMessagesProj/src/main/res/raw/tg_keep.xml:tools:keep="@string/com.google.firebase.crashlytics.*" />')
        # Credits, the configured payment website, and an upstream resource keep rule.
        ;;
      *) printf '%s\n' "$hit" ;;
    esac
  done
}

scan() {
  grep -rIn --exclude-dir=build --exclude-dir=jni --exclude-dir=.git \
    -e "$1" $2 2>/dev/null | grep -vE ':[0-9]+:[[:space:]]*(\*|//|/\*)' | filter_known_references
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
# Not plain "remoteConfig": Telegram itself has remoteConfigLoaded (app config over MTProto).
check "RemoteConfigHelper\|RemoteConfigConstants\|FirebaseRemoteConfig" "Firebase Remote Config"
check "arslan4k1390"                "upstream author endpoints"
check "ettacent\|nimarko"           "NimarkoGram infrastructure"
# Only build files: the word "huawei" also appears in stock Telegram vendor checks.
check "huawei\|agconnect"           "Huawei build wiring" "$GRADLE_SRC"

if [ "$FAIL" -ne 0 ]; then
  echo
  echo "check-clean: FAILED"
  exit 1
fi
echo
echo "check-clean: PASSED"
