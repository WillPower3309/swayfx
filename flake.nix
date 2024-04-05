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
    in
    {
      overlays.default = _: prev: { swayfx-unwrapped = self.packages.${prev.system}.swayfx-unwrapped; };

      packages = nixpkgs.lib.genAttrs targetSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        rec {
          swayfx-unwrapped =
            (pkgs.swayfx-unwrapped.override {
              # When the sway 1.9 rebase is finished, this will need to be overridden.
              # wlroots_0_16 = pkgs.wlroots_0_16;
            }).overrideAttrs
              (old: {
                version = "0.3.2-git";
                src = pkgs.lib.cleanSource ./.;
              });
          default = swayfx-unwrapped;
        }
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
              pkgs.wlroots_0_16
            ];
            nativeBuildInputs = with pkgs; [
              cmake
              wayland-scanner
            ];
            # Copy the nix version of wlroots into the project
            shellHook = with pkgs; ''
              (
                mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
                cp -R --no-preserve=mode,ownership ${wlroots_0_16.src} wlroots
              )'';
          };
        }
      );

      formatter = nixpkgs.lib.genAttrs targetSystems (system: {
        default = (pkgsFor system).nixfmt-rfc-style;
      });
    };
}
