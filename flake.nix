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
        # Qt Multimedia dlopens these at runtime (not linked into the binary).
        mediaRuntimeLibs = with pkgs; [
          pipewire
          libpulseaudio
        ];
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "alarmqt";
          version = "0.1.0";
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
          ];

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
          # Same runtime path when developing via `nix develop`
          LD_LIBRARY_PATH = lib.makeLibraryPath mediaRuntimeLibs;
        };
      });
}
