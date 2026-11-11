{
  description = "A fast, lightweight, and cross-platform raster image editor written in C++17 and Qt 6";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          thulium = pkgs.callPackage ./package.nix { };
          default = self.packages.${system}.thulium;
        }
      );

      apps = forAllSystems (system: {
        thulium = {
          type = "app";
          program = "${self.packages.${system}.thulium}/bin/thulium";
          meta = {
            description = "Thulium raster image editor";
          };
        };
        default = self.apps.${system}.thulium;
      });

      overlays.default = final: prev: {
        thulium = final.callPackage ./package.nix { };
      };

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.thulium ];
            packages = with pkgs; [
              gdb
            ];
            # Binaries built in the dev shell are not wrapped, so point Qt at the plugins that
            # match the Qt version they were linked against (otherwise the system-wide plugin
            # path is used and e.g. SVG icons fail to load).
            shellHook = ''
              export QT_PLUGIN_PATH=${pkgs.qt6.qtbase}/lib/qt-6/plugins:${pkgs.qt6.qtsvg}/lib/qt-6/plugins:${pkgs.qt6.qtimageformats}/lib/qt-6/plugins:${pkgs.qt6.qtwayland}/lib/qt-6/plugins
            '';
          };
        }
      );
    };
}
