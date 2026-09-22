#!/bin/bash

# Submit an app, pkg or dmg to Apple's notary service, wait for the verdict,
# and staple the ticket.
#
# This is built on `xcrun notarytool`. The previous version used `xcrun altool`,
# which Apple retired for notarization in November 2023 and which is no longer
# shipped with the Command Line Tools at all -- it did not merely stop working,
# the binary is gone.
#
# Credentials come from a notarytool keychain profile, created once with either
# an Apple ID and an app-specific password:
#
#   xcrun notarytool store-credentials "commsbus-notary" \
#       --apple-id <your-apple-id> --team-id VJ7JV9RU78
#
# or an App Store Connect API key, which suits a shared or CI setup because it
# is not tied to anyone's personal Apple ID:
#
#   xcrun notarytool store-credentials "commsbus-notary" \
#       --key AuthKey_XXXXXXXXXX.p8 --key-id XXXXXXXXXX --issuer <issuer-uuid>
#
# Set NOTARY_PROFILE to use a different profile name.
#
# The submission must already be signed with a Developer ID, with the hardened
# runtime enabled and a *secure timestamp*. Note that the CMake build signs with
# --timestamp=none for speed, so a plain ./buildcmake.sh product will be
# rejected -- codesign.sh re-signs with --timestamp before calling this.

set -u
set -e
set -o pipefail

PROFILE="${NOTARY_PROFILE:-commsbus-notary}"

RED="\033[1;31m"
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
RESET="\033[0m"

function die {
    echo -e "${RED}ERROR:${RESET} $1" >&2
    exit 1
}

# Pull one value out of a notarytool --output-format json response.
function json_get {
    plutil -extract "$2" raw -o - -- "$1" 2>/dev/null || true
}

do_submit=1
do_getresult=1
idfile=""
apps=()

while test "$#" -gt 0; do
  case "$1" in
   -*=*) optarg=$(echo "$1" | sed 's/[-_a-zA-Z0-9]*=//') ;;
      *) optarg= ;;
  esac
  case "$1" in
      --submit)      do_getresult=0 ;;
      --resume)      do_submit=0 ;;
      --submit=*)    do_getresult=0; idfile="$optarg" ;;
      --resume=*)    do_submit=0;    idfile="$optarg" ;;
      --primary-bundle-id=*)
          # altool needed this for pkg/dmg; notarytool works it out itself.
          # Accepted so existing callers (notarizedmg.sh) keep working.
          ;;
      --profile=*)   PROFILE="$optarg" ;;
      --help)
          echo "Usage: notarize-app.sh [options] MyBundle.app | MyInstaller.pkg | MyImage.dmg"
          echo "    Submits, waits for the verdict, and staples the ticket."
          echo
          echo "options:"
          echo "    --submit[=idfile]   submit and return immediately, saving the submission id"
          echo "    --resume[=idfile]   poll a saved submission id, and staple once accepted"
          echo "    --profile=NAME      notarytool keychain profile (default: ${PROFILE})"
          echo "    --primary-bundle-id=ID   accepted and ignored; notarytool does not need it"
          echo "    --trace             shell trace"
          echo
          echo "Credentials live in a notarytool keychain profile -- see the comments"
          echo "at the top of this script for how to create one."
          exit 0
          ;;
      --trace) set -x ;;
      --*)     die "wrong option: $1" ;;
      *)       apps+=("$1") ;;
  esac
  shift
done

xcrun notarytool --version >/dev/null 2>&1 || \
    die "notarytool not found. It ships with Xcode 13+ / recent Command Line Tools."

(( ${#apps[@]} > 0 )) || die "Missing argument: an .app, .pkg or .dmg to notarize."

is_app=1
for app in "${apps[@]}"; do
    [ -e "$app" ] || die "$app does not exist"
    case "${app##*.}" in
        pkg|dmg) is_app=0 ;;
        *) [ -d "$app" ] || die "$app is not a bundle directory" ;;
    esac
done

if (( $is_app == 0 )) && (( ${#apps[@]} > 1 )); then
    die "Only one pkg or dmg at a time."
fi

if (( $is_app )); then
    bundle_id=$(plutil -extract CFBundleIdentifier raw -o - -- "${apps[0]}/Contents/Info.plist" 2>/dev/null) \
        || die "Could not read CFBundleIdentifier from ${apps[0]}"
else
    bundle_id=$(basename "${apps[0]}")
fi
echo "Notarizing: ${apps[*]}  (${bundle_id})"

[ -n "$idfile" ] || idfile="/tmp/notarize-app-${bundle_id}.id"
outfile=$(mktemp "/tmp/notarize-app-${bundle_id}.XXXXXX.json")
trap 'rm -f "$outfile"' EXIT

submission_id=""
status=""

if (( $do_submit )); then
    if (( $is_app )); then
        # ditto, not `zip -r` -- zip mangles the symlinks inside a bundle.
        archive="/tmp/notarize-app-${bundle_id}.zip"
        rm -f "$archive"
        if (( ${#apps[@]} == 1 )); then
            ditto -c -k --keepParent "${apps[0]}" "$archive"
        else
            # --keepParent takes a single source, so stage them side by side
            stagedir=$(mktemp -d "/tmp/notarize-app-${bundle_id}.stage.XXXXXX")
            trap 'rm -f "$outfile"; rm -rf "$stagedir"' EXIT
            for a in "${apps[@]}"; do ditto "$a" "$stagedir/$(basename "$a")"; done
            ditto -c -k "$stagedir" "$archive"
        fi
    else
        archive="${apps[0]}"
    fi

    echo "Uploading $(basename "$archive")..."

    if (( $do_getresult )); then waitflag="--wait"; else waitflag="--no-wait"; fi

    if ! xcrun notarytool submit "$archive" \
            --keychain-profile "$PROFILE" \
            --output-format json \
            "$waitflag" > "$outfile"; then
        cat "$outfile" >&2
        die "notarytool submit failed. Is the '${PROFILE}' keychain profile set up?"
    fi

    submission_id=$(json_get "$outfile" id)
    [ -n "$submission_id" ] || { cat "$outfile" >&2; die "No submission id in the notarytool response."; }

    echo "$submission_id" > "$idfile"
    echo "Submission id: $submission_id  (saved to $idfile)"

    (( $do_getresult )) && status=$(json_get "$outfile" status)
else
    [ -f "$idfile" ] || die "$idfile not found, nothing to resume."
    submission_id=$(cat "$idfile")
    [ -n "$submission_id" ] || die "$idfile is empty."
    echo "Resuming submission $submission_id"
fi

# When resuming we poll ourselves; --wait already did this for us on submit.
if (( $do_getresult )) && [ -z "$status" ]; then
    for attempt in $(seq 1 60); do
        if xcrun notarytool info "$submission_id" \
                --keychain-profile "$PROFILE" \
                --output-format json > "$outfile" 2>/dev/null; then
            status=$(json_get "$outfile" status)
        fi
        [ "$status" != "In Progress" ] && [ -n "$status" ] && break
        echo -e "attempt #${attempt}: ${YELLOW}${status:-querying}${RESET}"
        sleep 20
    done
fi

if (( $do_getresult == 0 )); then
    echo "Submitted. Check it with: $0 --resume=${idfile} ${apps[*]}"
    exit 0
fi

if [ "$status" != "Accepted" ]; then
    echo -e "Notarization ${RED}${status:-failed}${RESET}" >&2
    echo "--- notary log ---" >&2
    xcrun notarytool log "$submission_id" --keychain-profile "$PROFILE" >&2 || true
    exit 42
fi

for app in "${apps[@]}"; do
    xcrun stapler staple "$app"
done

echo -e "${GREEN}Notarized and stapled:${RESET} ${apps[*]}"
