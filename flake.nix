{
  description = "Swayfx development environment";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    scenefx = {
      url = "github:wlrfx/scenefx";
      inputs.nixpkgs.follows = "nixpkgs";
    };
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
          (pkgs.swayfx-unwrapped.override {
            # This override block is currently empty, you might remove it
            # unless you intend to override function arguments later.
          }).overrideAttrs
            (old: {
              version = "0.5.2-git";
              src = pkgs.lib.cleanSource ./.;
              nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.cmake ];
              # Add wlroots_0_19 here
              buildInputs = old.buildInputs ++ [
                pkgs.scenefx
                pkgs.wlroots_0_19 # <-- Added this line
              ];
              providedSessions = [ pkgs.swayfx-unwrapped.meta.mainProgram ];
              patches = []; ## Consider if you need patches from the original derivation
              mesonFlags = let
                inherit (pkgs.lib.strings) mesonEnable mesonOption;
              in
                [
                  (mesonOption "sd-bus-provider" "libsystemd")
                  (mesonEnable "tray" true)
                  # You might need to explicitly enable fx if the default changed
                  # (mesonEnable "fx" true) # <-- Potentially add this if needed
                ];
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
          # inputsFrom propagates buildInputs, nativeBuildInputs etc. from the listed derivations
          # Adding wlroots and scenefx explicitly here is fine, but they are also included via inputsFrom
          inputsFrom = [
            self.packages.${pkgs.system}.swayfx-unwrapped
            # pkgs.wlroots_0_19 # Included via swayfx-unwrapped buildInputs now
            # pkgs.scenefx      # Included via swayfx-unwrapped buildInputs now
          ];
          # You still might want wlroots/scenefx here if you need tools/headers directly in the shell
          # outside of what swayfx uses.
          buildInputs = [
            pkgs.wlroots_0_19
            pkgs.scenefx
          ];
          packages = with pkgs; [
            gdb # for debugging
          ];
          shellHook = ''
            (
              # Copy the nix version of wlroots and scenefx into the project
              # This is useful if you want meson to use them as subprojects during manual dev/testing
              echo "Copying wlroots and scenefx sources to ./subprojects for dev environment..."
              mkdir -p "$PWD/subprojects" && cd "$PWD/subprojects"
              rm -rf wlroots scenefx # Clean previous copies if they exist
              cp -R --no-preserve=mode,ownership ${pkgs.wlroots_0_19.src} wlroots
              cp -R --no-preserve=mode,ownership ${pkgs.scenefx.src} scenefx
              echo "Done copying sources."
              cd "$OLDPWD"
            ) || echo "Failed to copy subproject sources."
          '';
        };
      });

      formatter = forEachSystem (pkgs: pkgs.nixfmt-rfc-style);
    };
}
