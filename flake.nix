# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "Simple system-tray alarm app (Qt6 / C++)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        inherit (pkgs) lib;

        versionBase = lib.strings.removeSuffix "\n" (builtins.readFile ./VERSION);
        gitRev = "${self.shortRev or self.dirtyShortRev or "dirty"}";
        isDev = lib.strings.hasInfix "-dev" versionBase;
        version =
          if isDev then
            "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
          else
            versionBase;

        # Qt Multimedia dlopens these at runtime (not linked into the binary).
        mediaRuntimeLibs = with pkgs; [
          pipewire
          libpulseaudio
        ];
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "alarmqt";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            qt6.wrapQtAppsHook
            pkg-config
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qtsvg          # for SVG icon
            qt6.qtmultimedia  # alarm sound
          ] ++ mediaRuntimeLibs;

          # So QSoundEffect's PipeWire/Pulse backends can dlopen successfully.
          qtWrapperArgs = [
            "--prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath mediaRuntimeLibs}"
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DPROJECT_VERSION_FULL=${version}"
            "-DALARMQT_BUILD_TESTS=ON"
          ];

          doCheck = true;
          # QTest may touch GUI plugins; force offscreen.
          preCheck = ''
            export QT_QPA_PLATFORM=offscreen
          '';

          meta = with lib; {
            description = "Keyboard-friendly system-tray alarm / reminder";
            homepage = "https://github.com/Grumbel/alarmqt";
            license = licenses.gpl3Plus;
            platforms = platforms.linux;
            mainProgram = "alarmqt";
          };
        };

        apps.default = flake-utils.lib.mkApp {
          drv = self.packages.${system}.default;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];
          buildInputs = with pkgs; [
            qt6.qttools
            gdb
            clang-tools
          ];
          LD_LIBRARY_PATH = lib.makeLibraryPath mediaRuntimeLibs;
        };
      });
}
