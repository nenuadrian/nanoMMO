#!/bin/sh
# Checks that this machine has what building and running nanoMMO takes: the
# tools, the libraries, Docker, the ports the servers listen on and the
# servers' certificates. It says what to do about each thing that's missing,
# and ends with one command that installs every missing package, with Homebrew
# on macOS and apt on Ubuntu. It exits 1 when the game can't build or run
# without something.
set -u
cd "$(dirname "$0")/.."

mac=false
[ "$(uname)" = Darwin ] && mac=true
missing=0
packages=""

ok() { printf '  \033[32m✓\033[0m %s\n' "$1"; }
note() { printf '  \033[33m!\033[0m %s\n      %s\n' "$1" "$2"; }
# Something the game needs and this machine hasn't got, and what to do.
bad() {
  printf '  \033[31m✗\033[0m %s\n      %s\n' "$1" "$2"
  missing=1
}
# A missing package, by its Homebrew name, then its apt name.
want() {
  if $mac; then package=$2; else package=$3; fi
  if $mac; then bad "$1" "brew install $package"; else bad "$1" "sudo apt install $package"; fi
  packages="$packages $package"
}
has() { command -v "$1" > /dev/null; }
# Whether version $1 is $2 or newer.
at_least() {
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" = "$2" ]
}
# Homebrew keeps mysql-client and OpenSSL where pkg-config doesn't look, so
# point it there, as CMakeLists.txt does for CMake.
if $mac && has brew; then
  for formula in mysql-client openssl@3; do
    prefix=$(brew --prefix "$formula" 2> /dev/null) &&
      PKG_CONFIG_PATH="$prefix/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
  done
  export PKG_CONFIG_PATH
fi

echo "Building"
if has git; then
  ok "git $(git --version | cut -d' ' -f3)"
else
  want "git: not found" git git
fi
if ! has cmake; then
  want "cmake: not found" cmake cmake
else
  version=$(cmake --version | head -n 1 | cut -d' ' -f3)
  if at_least "$version" 3.28; then
    ok "cmake $version"
  else
    bad "cmake $version: 3.28 or newer, please" \
      "Ubuntu 22.04's is too old: sudo snap install cmake --classic"
  fi
fi
if has ninja; then
  ok "ninja $(ninja --version)"
else
  want "ninja: not found" ninja ninja-build
fi
# The compiler the release preset builds with, which CXX names, if it's set.
# The game is C++23, and two of the parts it uses came late to the compilers
# (GCC 14, Clang 19 and Xcode 16.3): a member function that takes its object
# as a parameter, and a subscript operator with two indexes. So we ask the
# compiler to compile both.
compiler=${CXX:-c++}
if ! has "$compiler"; then
  if $mac; then
    bad "a C++ compiler: not found" "xcode-select --install"
  else
    want "a C++ compiler: not found" "" build-essential
  fi
elif printf 'struct S {\n  void f(this S&) {}\n  int operator[](int, int) const { return 0; }\n};\n' |
  "$compiler" -std=c++23 -fsyntax-only -x c++ - 2> /dev/null; then
  ok "$("$compiler" --version | head -n 1)"
elif $mac; then
  bad "$("$compiler" --version | head -n 1): too old for C++23" \
    "Xcode 16.3 or newer, from the App Store"
else
  bad "$("$compiler" --version | head -n 1): too old for C++23" \
    "sudo apt install g++-14, then export CXX=g++-14 before CMake's first run"
fi
if ! has pkg-config; then
  want "pkg-config: not found" pkgconf pkg-config
  # Without it there's no telling which libraries are here, so the command at
  # the end installs them all: a package manager skips what it has.
  if $mac; then
    note "the libraries: can't tell without pkg-config" \
      "brew install mysql-client libsodium openssl@3"
    packages="$packages mysql-client libsodium openssl@3"
  else
    note "the libraries: can't tell without pkg-config" \
      "sudo apt install libmysqlclient-dev libsodium-dev libssl-dev xorg-dev libgl1-mesa-dev"
    packages="$packages libmysqlclient-dev libsodium-dev libssl-dev xorg-dev libgl1-mesa-dev"
  fi
else
  if pkg-config --exists mysqlclient; then
    ok "MySQL's client library"
  else
    want "MySQL's client library: not found" mysql-client libmysqlclient-dev
  fi
  if pkg-config --exists libsodium; then
    ok "libsodium $(pkg-config --modversion libsodium)"
  else
    want "libsodium: not found" libsodium libsodium-dev
  fi
  if pkg-config --atleast-version=3 openssl; then
    ok "OpenSSL $(pkg-config --modversion openssl)"
  else
    want "OpenSSL 3: not found" openssl@3 libssl-dev
  fi
  # raylib draws with OpenGL, in a window from X11, on Linux.
  if ! $mac; then
    if pkg-config --exists x11 gl; then
      ok "X11 and OpenGL"
    else
      want "X11 and OpenGL's headers: not found" "" "xorg-dev libgl1-mesa-dev"
    fi
  fi
fi

echo "Running"
if ! has docker; then
  if $mac; then
    bad "docker: not found" \
      "Docker Desktop: https://docs.docker.com/desktop/setup/install/mac-install/"
  else
    want "docker: not found" "" "docker.io docker-compose-v2 docker-buildx"
  fi
elif ! output=$(docker info 2>&1 > /dev/null); then
  case $output in
    *"permission denied"*)
      bad "docker: not yours to use" \
        "sudo usermod -aG docker \$USER, then log out and in again" ;;
    *)
      bad "docker: not running" \
        "start Docker Desktop, or: sudo systemctl start docker" ;;
  esac
else
  ok "docker $(docker version --format '{{.Server.Version}}')"
  # The game's compose.yaml uses !override, which came in Compose 2.24.
  version=$(docker compose version --short 2> /dev/null)
  if [ -z "$version" ]; then
    want "docker compose: not found" docker-compose docker-compose-v2
  elif at_least "${version#v}" 2.24; then
    ok "docker compose $version"
  else
    want "docker compose $version: 2.24 or newer, please" \
      docker-compose docker-compose-v2
  fi
  if docker buildx version > /dev/null 2>&1; then
    ok "docker buildx"
  else
    want "docker buildx: not found" docker-buildx docker-buildx
  fi
fi
# Without a screen, the client's tests open their windows on a virtual one.
if ! $mac && [ -z "${DISPLAY:-}" ]; then
  if has xvfb-run; then
    ok "xvfb-run, for the client's tests without a screen"
  else
    note "no screen, and no xvfb-run for the client's tests" \
      "sudo apt install xvfb, then: xvfb-run -a ctest --preset release"
  fi
fi

echo "Ports"
# What listens on a port, if anything: a container's name, or a program's.
listener() {
  if has docker; then
    name=$(docker ps --filter "publish=$1" --format '{{.Names}}' 2> /dev/null)
    [ -n "$name" ] && echo "$name" | head -n 1 && return
  fi
  if has ss; then
    line=$(ss -Hltnup "sport = :$1" 2> /dev/null | head -n 1)
    [ -n "$line" ] || return 0
    name=$(echo "$line" | sed -n 's/.*users:(("\([^"]*\)".*/\1/p')
    echo "${name:-a program of another user}"
  elif has lsof; then
    lsof -nP -iTCP:"$1" -sTCP:LISTEN 2> /dev/null | awk 'NR == 2 { print $1 }'
  fi
}
for service in 3306:MySQL 6379:Redis 8500:Consul 7778:"the login server" \
  7777:"the game server" 9464:"the game server's metrics"; do
  port=${service%%:*}
  who=$(listener "$port")
  case $who in
    "") ok "$port, for ${service#*:}, is free" ;;
    nanommo*) ok "$port, for ${service#*:}, is nanoMMO's own: $who" ;;
    *) note "$port, for ${service#*:}, is taken by $who" \
      "stop it, or ${service#*:} can't start" ;;
  esac
done

echo "Certificates"
if [ -f tls/server.crt ]; then
  names=$(openssl x509 -in tls/server.crt -noout -text |
    sed -n '/Subject Alternative Name/{n;s/^ *//;p;}')
  ok "tls/server.crt names $names"
else
  note "none yet" "scripts/certificates.sh makes them, before the servers start"
fi

if [ -n "$packages" ]; then
  echo
  if $mac; then
    echo "To install the missing packages: brew install$packages"
  else
    echo "To install the missing packages: sudo apt update && sudo apt install$packages"
  fi
fi
exit $missing
