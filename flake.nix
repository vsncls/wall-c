{
  description = "wall-c reproducible build and Lean/CompCert verification environment";

  inputs = {
    # flake.lock is the actual reproducibility pin. Keep this on a release branch
    # rather than an unbounded nixpkgs channel once the first lock is generated.
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      pkgsFor = system:
        import nixpkgs {
          inherit system;

          # CompCert is packaged as unfree in nixpkgs because of its license.
          # Keep the exception narrow rather than enabling all unfree packages.
          config.allowUnfreePredicate = pkg:
            builtins.elem (nixpkgs.lib.getName pkg) [ "compcert" ];
        };
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "wall-c";
            version = "0.3.0";
            src = self;

            # Avoid ambient locale/timezone differences and archive timestamps.
            # These settings do not by themselves prove reproducibility; CI must
            # compare outputs from independent builds.
            SOURCE_DATE_EPOCH = "1";
            ZERO_AR_DATE = "1";
            LC_ALL = "C";
            TZ = "UTC";

            strictDeps = true;

            buildPhase = ''
              runHook preBuild
              make release CC="$CC"
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              make test CC="$CC"
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              make install CC="$CC" PREFIX="$out"
              runHook postInstall
            '';
          };

          # Expose the exact CompCert package selected by flake.lock so CI and
          # proof tooling do not depend on whichever ccomp happens to be in PATH.
          compcert = pkgs.compcert;
        });

      devShells = forAllSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              clang
              gcc
              gnumake
              compcert
              lean4
            ];

            SOURCE_DATE_EPOCH = "1";
            ZERO_AR_DATE = "1";
            LC_ALL = "C";
            TZ = "UTC";

            shellHook = ''
              echo "wall-c verification shell"
              echo "  C compiler sanity: gcc / clang"
              echo "  verified compiler: ccomp (CompCert)"
              echo "  prover: lean / lake"
            '';
          };

          proof = pkgs.mkShell {
            packages = with pkgs; [
              compcert
              lean4
              gnumake
            ];

            SOURCE_DATE_EPOCH = "1";
            ZERO_AR_DATE = "1";
            LC_ALL = "C";
            TZ = "UTC";
          };
        });

      checks = forAllSystems (system: {
        wall-c = self.packages.${system}.default;
      });
    };
}
