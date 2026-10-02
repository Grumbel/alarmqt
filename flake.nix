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
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          meta = with pkgs.lib; {
            description = "Keyboard-friendly system-tray alarm / reminder";
            homepage = "https://github.com/example/alarmqt";
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
            qt6.qttools        # designer, lupdate, …
            gdb
            clang-tools
          ];
        };
      });
}
