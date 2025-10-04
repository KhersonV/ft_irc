#!/usr/bin/env bash
set -euo pipefail

APPDIR="${APPDIR:-$HOME/apps/weechat}"
BINDIR="${BINDIR:-$HOME/bin}"
PLUGIN_SUBDIR_1="usr/lib/x86_64-linux-gnu/weechat/plugins"
PLUGIN_SUBDIR_2="usr/lib/weechat/plugins"
EXTRA_DEPS=("libgcrypt20" "libgnutls30")

need() { command -v "$1" >/dev/null 2>&1 || { echo "Error: '$1' not found"; exit 1; }; }
latest_deb() { ls -1t $1 2>/dev/null | head -n1 || true; }  # берём самый свежий .deb по времени

echo "[0/7] Checking tools…"
need dpkg-deb
if command -v apt >/dev/null 2>&1; then PM="apt"; else need apt-get; PM="apt-get"; fi

mkdir -p "${APPDIR}" "${BINDIR}"

echo "[1/7] Downloading WeeChat .deb packages…"
if ! ls -1 weechat-core_*.deb >/dev/null 2>&1 || ! ls -1 weechat-curses_*.deb >/dev/null 2>&1 || ! ls -1 weechat-plugins_*.deb >/dev/null 2>&1; then
  if [ "$PM" = "apt" ]; then
    apt download weechat-core weechat-curses weechat-plugins
  else
    apt-get download weechat-core weechat-curses weechat-plugins
  fi
else
  echo "  -> found existing weechat-core/curses/plugins .debs, will reuse them"
fi

DEB_CORE=$(latest_deb "weechat-core_*.deb")
DEB_CURSES=$(latest_deb "weechat-curses_*.deb")
DEB_PLUGINS=$(latest_deb "weechat-plugins_*.deb")
[ -n "$DEB_CORE" ] && [ -n "$DEB_CURSES" ] && [ -n "$DEB_PLUGINS" ] || { echo "Error: missing .deb files"; exit 1; }

echo "[2/7] Extracting into ${APPDIR}…"
dpkg-deb -x "$DEB_CORE"   "${APPDIR}"
dpkg-deb -x "$DEB_CURSES" "${APPDIR}"
dpkg-deb -x "$DEB_PLUGINS" "${APPDIR}"

echo "[3/7] Optionally fetching runtime deps (if available)…"
for dep in "${EXTRA_DEPS[@]}"; do
  if [ "$PM" = "apt" ]; then
    if apt download "$dep" >/dev/null 2>&1; then
      deb=$(latest_deb "${dep}_*.deb"); [ -n "$deb" ] && dpkg-deb -x "$deb" "${APPDIR}" || true
    fi
  else
    if apt-get download "$dep" >/dev/null 2>&1; then
      deb=$(latest_deb "${dep}_*.deb"); [ -n "$deb" ] && dpkg-deb -x "$deb" "${APPDIR}" || true
    fi
  fi
done

echo "[4/7] Detecting plugin directory…"
PLUGIN_DIR=""
if [ -d "${APPDIR}/${PLUGIN_SUBDIR_1}" ] && ls "${APPDIR}/${PLUGIN_SUBDIR_1}"/*.so >/dev/null 2>&1; then
  PLUGIN_DIR="${APPDIR}/${PLUGIN_SUBDIR_1}"
elif [ -d "${APPDIR}/${PLUGIN_SUBDIR_2}" ] && ls "${APPDIR}/${PLUGIN_SUBDIR_2}"/*.so >/dev/null 2>&1; then
  PLUGIN_DIR="${APPDIR}/${PLUGIN_SUBDIR_2}"
else
  echo "Error: plugins directory not found in ${APPDIR}"
  echo "Searched:"
  echo "  - ${APPDIR}/${PLUGIN_SUBDIR_1}"
  echo "  - ${APPDIR}/${PLUGIN_SUBDIR_2}"
  exit 1
fi
echo "  -> plugins at: ${PLUGIN_DIR}"

echo "[5/7] Creating launcher ${BINDIR}/weechat…"
cat > "${BINDIR}/weechat" <<EOF
#!/usr/bin/env bash
set -euo pipefail
APPDIR="${APPDIR}"
export LD_LIBRARY_PATH="\${APPDIR}/usr/lib/x86_64-linux-gnu:\${APPDIR}/usr/lib:\${LD_LIBRARY_PATH:-}"
exec "\${APPDIR}/usr/bin/weechat-curses" "\$@"
EOF
chmod +x "${BINDIR}/weechat"

case ":$PATH:" in
  *:"${BINDIR}":*) ;;
  *) export PATH="${BINDIR}:$PATH" ;;
esac

echo "[6/7] Initializing plugin path & autoload in WeeChat config…"
export LD_LIBRARY_PATH="${APPDIR}/usr/lib/x86_64-linux-gnu:${APPDIR}/usr/lib:${LD_LIBRARY_PATH:-}"
"${APPDIR}/usr/bin/weechat-curses" -r "/set weechat.plugin.path \"${PLUGIN_DIR}\"; /set weechat.plugin.autoload \"*\"; /save; /quit" || true

echo "[7/7] Done."
echo
echo "Launch WeeChat with: weechat"
echo "If 'weechat' not found, run: export PATH=\"${BINDIR}:\$PATH\""


# /eval /set weechat.plugin.path "${env:HOME}/apps/weechat/usr/lib/x86_64-linux-gnu/weechat/plugins"
# /plugin autoload

# instruction for WeeChat

# start nc with small c (nc -c 127.0.0.1 6667)
# start weechat with -d flag for multiple instances

# weechat -d ~/.weechat2
# weechat -d ~/.weechat1
# ...

# commands:

# A) join server / create user

# /server add ftirc_a 127.0.0.1/6667 -password=otherpass
# /set irc.server.ftirc_a.password pass
# /set irc.server.ftirc_a.nicks "alice,alice_"
# /set irc.server.ftirc_a.username "alice"
# /set irc.server.ftirc_a.realname "Alice Test"
# /set irc.server.ftirc_a.tls off
# /connect ftirc_a

# nc -c 127.0.0.1 6667
# PASS otherpass
# NICK alice
# USER alice 0 * :Alice Test

# //////

# B) create / join channel
# /join #testChnl
# JOIN #testChnl 

# /join #teschnl chnlpass
# JOIN #testchnl chnlpass

# ///////

# C) messages
# /msg #testChnl hello world
# /query alice hi
# /msg alice hi
# PRIVMSG #testChnl :hello world
# PRIVMSG alice :hi

# //////

# D) CHANNEL COMMANDS

# 1) topic
# /topic #testChnl some topic
# TOPIC #testChnl :some topic

# 2) KICK
# /kick #testChnl bob msgReason
# KICK #testChnl bob :msgReason

# 3) INVITE
# /invite bob #testChnl
# INVITE bob #testChnl

# 4) MODE (+ enable, - disable)
# - i (invite only mode)
# /mode #testChnl +i
# MODE #testChnl +i
# - t (TOPIC command mode)
# /mode #testChnl +t
# MODE #testChnl +t
# - k (channel key (password))
# /mode #testChnl +k secret
# MODE #testChnl +k secret
# - o (Give/take operator privilege)
# /mode #testChnl +o bob
# MODE #testChnl +o bob
# - l (user limits)
# /mode #testChnl +l 2
# MODE #testChnl +l 2

# 5) leave chnl, quit
# /part #test
# /quit Bye

# PART #test :Bye
# QUIT :Bye

# 6) sending files (only with via clients, will not work with nc)
# uses privmsg to send request and client takes it from there
# it may be beneficial to set download path
# /set xfer.file.download_path "absPath"

# /dcc send <clientNick> <absfilepath>
