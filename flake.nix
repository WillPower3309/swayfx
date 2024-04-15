{
  description = "Swayfx development environment";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    scenefx.url = "github:ozwaldorf/scenefx";
  };
  outputs =
    {
      self,
      nixpkgs,
      scenefx,
      ...
    }:
    let
      pkgsFor =
        system:
        import nixpkgs {
          inherit system;
          overlays = [ scenefx.overlays.insert ];
        };
      targetSystems = [
        "aarch64-linux"
        "x86_64-linux"
      ];
      mkPackage = pkgs: {
        swayfx-unwrapped =
          (pkgs.swayfx-unwrapped.override { wlroots_0_16 = pkgs.wlroots_0_17; }).overrideAttrs
            (old: {
              version = "0.3.2-git";
              src = pkgs.lib.cleanSource ./.;
              nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.cmake ];
              buildInputs = old.buildInputs ++ [ pkgs.scenefx ];
            });
      };
    in
    {
      overlays = rec {
        default = insert;
        # Override onto the input nixpkgs
        override = _: prev: mkPackage prev;
        # Insert using the locked nixpkgs
        insert = _: prev: mkPackage (pkgsFor prev.system);
      };

      packages = nixpkgs.lib.genAttrs targetSystems (
        system: (mkPackage (pkgsFor system) // { default = self.packages.${system}.swayfx-unwrapped; })
      );

      devShells = nixpkgs.lib.genAttrs targetSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            name = "swayfx-shell";
            inputsFrom = [
              self.packages.${system}.swayfx-unwrapped
              pkgs.wlroots_0_17
              pkgs.scenefx
            ];
            nativeBuildInputs = with pkgs; [
              wayland-scanner
              hwdata # for wlroots
            ];
            # Copy the nix version of wlroots into the project
            shellHook = ''
              (
                mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
                cp -R --no-preserve=mode,ownership ${pkgs.wlroots_0_17.src} wlroots
                cp -R --no-preserve=mode,ownership ${pkgs.scenefx.src} scenefx
              )'';
          };
        }
      );

      formatter = nixpkgs.lib.genAttrs targetSystems (system: (pkgsFor system).nixfmt-rfc-style);
    };
}
