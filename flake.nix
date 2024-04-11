{
  description = "Swayfx development environment";
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs =
    { self, nixpkgs, ... }:
    let
      pkgsFor = system: import nixpkgs { inherit system; };
      targetSystems = [
        "aarch64-linux"
        "x86_64-linux"
      ];
      mkPackage = pkgs: {
        swayfx-unwrapped =
          (pkgs.swayfx-unwrapped.override {
            # When the sway 1.9 rebase is finished, this will need to be overridden.
            # wlroots_0_16 = pkgs.wlroots_0_16;
          }).overrideAttrs
            (old: {
              version = "0.3.2-git";
              src = pkgs.lib.cleanSource ./.;
            });
      };
    in
    {
      overlays = rec {
        default = override;
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
            ];
            nativeBuildInputs = with pkgs; [
              cmake
              wayland-scanner
            ];
            # Copy the nix version of wlroots into the project
            shellHook = with pkgs; ''
              (
                mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
                cp -R --no-preserve=mode,ownership ${wlroots_0_17.src} wlroots
              )'';
          };
        }
      );

      formatter = nixpkgs.lib.genAttrs targetSystems (system: (pkgsFor system).nixfmt-rfc-style);
    };
}
