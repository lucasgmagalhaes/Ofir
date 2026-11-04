# Ofir for Visual Studio Code

This extension identifies `.of` files as **Ofir**, highlights basic syntax,
configures comments and brackets, and supplies the Ofir logo as the language
and extension icon. It does not provide diagnostics or language-server features.

## Install locally

From `editors/vscode/`:

```sh
npx --yes @vscode/vsce package --out ofir-language.vsix
code --install-extension ofir-language.vsix
```

Reload the VS Code window after installation. Open a `.of` file and check that
the language indicator in the status bar says **Ofir**. The logo appears as its
file icon when the active file icon theme supports contributed language icons.
The extension also uses the logo in the Extensions view.

The source logo is `docs/assets/logo.png`; this extension keeps a copy in
`icons/ofir.png` so it can be packaged independently.
