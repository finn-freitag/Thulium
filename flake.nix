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
          };
        }
      );
    };
}
