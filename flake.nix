{
  description = "Swayfx development environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";

    systems.url = "github:nix-systems/default-linux";

    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };

    scenefx = {
      url = "github:wlrfx/scenefx";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
    };
  };

  outputs = { self, nixpkgs, flake-utils, scenefx, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        scenefx-git = scenefx.packages.${system}.scenefx-git;

      in {
        packages = rec {
          swayfx-unwrapped-git = pkgs.swayfx-unwrapped.overrideAttrs(old: {
            version = "git";
            src = pkgs.lib.cleanSource ./.;
            nativeBuildInputs = with pkgs; [
              meson
              ninja
              pkg-config
              wayland-scanner
              scdoc
            ];
            buildInputs = with pkgs; [
              libGL
              wayland
              libxkbcommon
              pcre2
              json_c
              libevdev
              pango
              cairo
              libinput
              gdk-pixbuf
              librsvg
              wayland-protocols
              libdrm
              xorg.xcbutilwm
              wlroots_0_19
            ] ++ [ scenefx-git ];
          });

          default = swayfx-unwrapped-git;
        };

        devShells.default = pkgs.mkShell {
          name = "swayfx-shell";
          inputsFrom = [
            self.packages.${system}.swayfx-unwrapped-git
            scenefx-git
          ];
          packages = [ pkgs.gdb ]; # add debugging packages
          shellHook = ''
            (
              mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
              if [ ! -d scenefx ]; then
                echo "Copying scenefx to ./subprojects..."
                cp -R --no-preserve=mode,ownership ${scenefx-git.src} scenefx
              fi
              cd "$OLDPWD"
            ) || echo "Failed to copy subproject sources."
          '';
          hardeningDisable = [ "fortify" ];
        };

        formatter = pkgs.nixfmt-rfc-style;
      }
    );
}

