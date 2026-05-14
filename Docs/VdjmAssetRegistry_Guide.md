# VdjmAssetRegistry Guide

`VdjmAssetRegistry` is the central asset ledger for the plugin. Its first job is not to force runtime loading. Its first job is to make confirmed assets visible, classifiable, searchable, and verifiable from one JSON file.

## Schema Roots

- `schema_version`: JSON schema version. Version `1` is the initial registry format.
- `meta`: Human-facing information about this registry.
- `defines`: Preprocessor-style replacements. Use `!{DEFINE_NAME}` only.
- `paths`: Physical roots, external paths, and named root shortcuts.
- `virtuals`: Virtual folder tree. It is a classification hierarchy, not a file path.
- `asset_types`: Allowed asset types and their default classes/extensions.
- `assets`: Registered asset entries. This is the actual ledger.
- `requirements`: Validation rules that can target virtual folders.

## Defines

Defines are expanded before the registry is interpreted.

```json
"defines": {
  "TYPE_WIDGET_BP": "widget_bp",
  "DIR_PREVIEW": "UI/Preview"
}
```

Use them with `!{...}`:

```json
"type": "!{TYPE_WIDGET_BP}",
"relative_path": "!{DIR_PREVIEW}/WBP_MediaCard"
```

Rules:

- Use `!{DEFINE_NAME}` only.
- Define names should use `UPPER_SNAKE_CASE`.
- Define names may contain letters, digits, and `_`.
- Define values may reference other defines.
- Missing defines, unclosed tokens, and circular references are validation errors.

## Virtual Folders

Virtual paths use `#{...}` and are resolved after define expansion.

```json
"virtual_path": "#{recorder-preview-carousel}"
```

The path above points into this tree:

```json
"virtuals": {
  "key": "root",
  "children": [
    {
      "key": "recorder",
      "children": [
        {
          "key": "preview",
          "children": [
            {
              "key": "carousel",
              "children": []
            }
          ]
        }
      ]
    }
  ]
}
```

Virtual folders are not physical folders. They let scattered physical assets appear together in one classification.

## Asset Keys

Asset keys are generated, not hand-authored:

```text
type:root:relative_path
```

Example:

```text
widget_bp:preview-ui:WBP_MediaCard
```

This keeps the registry from needing separate aliases. Short reusable strings belong in `defines`.

## Importance

- `required`: Missing file is an error.
- `recommended`: Missing file is a warning.
- `optional`: Missing file is tolerated.

## Editor/Dev Workflow

1. Update or create `Config/VdjmAssetRegistry.json`.
2. Run `LoadDefaultRegistry` and `ValidateRegistry`.
3. Run `ScanDefaultRegistry` with registration enabled to add discovered assets.
4. Read the generated JSON and summary.
5. Manually classify important assets with `virtual_path`, `importance`, `tags`, and `meta`.

The registry save path is the plugin config file in editor/development workflows. Packaged builds should treat this registry as read-only unless a separate writable path is introduced.
