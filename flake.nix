{
  description = "Swayfx development environment";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    scenefx.url = "github:wlrfx/scenefx";
  };
  outputs =
    {
      self,
      nixpkgs,
      scenefx,
      ...
    }:
    let
      mkPackage = pkgs: {
        swayfx-unwrapped =
          (pkgs.swayfx-unwrapped.override { wlroots = pkgs.wlroots_0_17; }).overrideAttrs
            (old: {
              version = "0.4.0-git";
              src = pkgs.lib.cleanSource ./.;
              nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.cmake ];
              buildInputs = old.buildInputs ++ [ pkgs.scenefx ];
            });
      };

      targetSystems = [
        "aarch64-linux"
        "x86_64-linux"
      ];
      pkgsFor =
        system:
        import nixpkgs {
          inherit system;
          overlays = [ scenefx.overlays.insert ];
        };
      forEachSystem = f: nixpkgs.lib.genAttrs targetSystems (system: f (pkgsFor system));
    in
    {
      overlays = rec {
        default = insert;
        # Insert using the locked nixpkgs. Can be used with any nixpkgs version.
        insert = _: prev: mkPackage (pkgsFor prev.system);
        # Override onto the input nixpkgs. Users *MUST* have a scenefx overlay
        # used before this overlay, otherwise pkgs.scenefx will be unavailable
        override = _: prev: mkPackage prev;
      };

      packages = forEachSystem (
        pkgs: (mkPackage pkgs // { default = self.packages.${pkgs.system}.swayfx-unwrapped; })
      );

      devShells = forEachSystem (pkgs: {
        default = pkgs.mkShell {
          name = "swayfx-shell";
          inputsFrom = [
            self.packages.${pkgs.system}.swayfx-unwrapped
            pkgs.wlroots_0_17
            pkgs.scenefx
          ];
          packages = with pkgs; [
            gdb # for debugging
          ];
          shellHook = ''
            (
              # Copy the nix version of wlroots and scenefx into the project
              mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
              cp -R --no-preserve=mode,ownership ${pkgs.wlroots_0_17.src} wlroots
              cp -R --no-preserve=mode,ownership ${pkgs.scenefx.src} scenefx
            )'';
        };
      });

      formatter = forEachSystem (pkgs: pkgs.nixfmt-rfc-style);
    };
}
