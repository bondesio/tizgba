#!/bin/sh
# Deferred Q60A functional checks for TizGBA. DO NOT run during development:
# this script launches the app on screen. Run only when the TV is free and
# after the signed TPK and WGT have been installed by the native-app procedure.
#
#   tools/deferred-tv-checks.sh          launch and print initial diagnostics
#   tools/deferred-tv-checks.sh --logs   re-read diagnostics after hands-on play
set -eu
cd "$(dirname "$0")/.."
D=/opt/usr/home/owner/apps_rw/org.pablo.tizgba/data
APP=org.pablo.tizgba
LAUNCHER=GBALAUNCH1.Launcher

logs() {
    echo "== TizGBA runtime log"
    ssh q60a-tv-root "cat $D/tizgba-runtime.log 2>/dev/null || echo '(no app log yet)'"
    echo "== input selection and active marker"
    ssh q60a-tv-root "cat $D/tizgba-input.conf 2>/dev/null || echo '(no controller selected)'; cat $D/tizgba-active 2>/dev/null || echo '(app not active)'"
    echo "== watcher grants"
    ssh q60a-tv-root 'cat /run/tizgba-input-access.event /run/tizgba-input-access.event2 2>/dev/null || true; tail -12 /run/tizgba-input-access.log 2>/dev/null || true'
    echo "== current USB mounts"
    ssh q60a-tv-root 'grep -E " /opt/media/| /run/media/" /proc/mounts || echo "(no USB drive mounted)"'
}

if [ "${1:-}" = "--logs" ]; then logs; exit 0; fi
if [ "${1:-}" != "" ]; then echo "usage: $0 [--logs]" >&2; exit 2; fi

echo "== Required: the TV must be free; this will launch TizGBA on screen."
echo "== UEP status (native UI needs uep_toggle loaded on this firmware)"
ssh q60a-tv-root 'grep -q "^uep_toggle " /proc/modules && echo "uep_toggle loaded" || echo "UEP NOT DISABLED: reboot and wait for root startup"'
echo "== Check package registrations before launch"
ssh q60a-tv-root "pkginfo --app $APP 2>/dev/null | head -20; pkginfo --app $LAUNCHER 2>/dev/null | head -20"
echo "== USB mounts visible to the system (the app must also see them via Storage API)"
ssh q60a-tv-root 'grep -E " /opt/media/| /run/media/" /proc/mounts || echo "(no USB drive mounted)"'
echo "== Launch TizGBA through its Home launcher"
ssh q60a-tv-root "app_launcher -s $LAUNCHER"
sleep 12
ssh q60a-tv-root "app_launcher -S | grep -E 'tizgba|GBALAUNCH1' || echo '(not listed)'; pgrep -f /opt/usr/apps/$APP/bin/tizgba >/dev/null && echo 'native process running' || echo 'native process not found'"
logs
cat <<'EOF'
== Hands-on checks on the TV
   1. Confirm the first library row is Exit TizGBA and ROMs from internal and
      connected USB storage appear with the expected USB tag.
   2. In Settings > Controllers, select the physical pad and run the guided
      input check. Confirm D-pad buttons and analog axes are separate. Enable
      exclusive capture and verify another app no longer receives pad input.
   3. Launch an owned GBA ROM. Check picture, buttons, audio, Back-to-library,
      and relaunch. For a battery-backed game, save, leave, relaunch and confirm
      persistence. Avoid using personal save data as a test fixture.
   4. Try each screen-size/filter option and watch for pacing issues. The TV
      runtime log should show any filter-budget downgrade.
   5. Pause or leave the app and check that the active marker disappears and
      the watcher restores the selected event node. Resume and check recapture.
   6. Run this script again with --logs and preserve the output with the
      results. Do not report ROM, save, controller or filter behavior as
      verified until these steps are observed.
EOF
