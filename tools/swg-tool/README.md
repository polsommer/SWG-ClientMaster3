# swg-tool CLI

`swg-tool` streamlines common workflows for the modernised God Client toolchain: validating plugin manifests, generating
navmeshes, and publishing content bundles. Each subcommand intentionally mirrors the automation goals from the modernization
plan so teams can begin scripting repeatable tasks immediately.

## Usage

```bash
python -m swg_tool --help
```

### Validate plugin manifests

```bash
python -m swg_tool validate plugin/examples/world_builder_procedural/plugin.json
```

### Generate a navmesh prototype

```bash
python -m swg_tool generate-navmesh --terrain data/heightmap.txt --output build/navmesh.json
```

### Publish a content pack

```bash
python -m swg_tool publish --content ./dist/content --destination ./publish/releases
```

### Generate a response file

```bash
python -m swg_tool generate-response \
  --source ./dist/content \
  --source ./dist/localized/en \
  --destination build/generated_content.rsp
```

### Build a `.tre` or `.tres` archive

```bash
python -m swg_tool build-tree --response build/content.rsp --output publish/snapshot.tres --encrypt --passphrase "secret"
```

You can also have the command generate the response file on the fly from directories or individual files:

```bash
python -m swg_tool build-tree \
  --source ./dist/content \
  --source ./dist/localized/en \
  --response build/generated_content.rsp \
  --output publish/snapshot.tre
```

When `--source` is provided the inputs are walked recursively, a deterministic response file is produced (either at `--response` or a temporary location), and then handed to the legacy builder. Use `--entry-root` to control how tree entry paths are calculated when the defaults are not suitable.

This wraps the legacy `TreeFileBuilder` executable so modern automation can generate encrypted archives that the SWG+ client can consume.

See `python -m swg_tool <command> --help` for a full list of arguments.
