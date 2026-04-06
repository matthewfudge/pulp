---
name: inspect
description: Inspect the view hierarchy and widget state
---

Launch the component inspector to debug the view tree, widget state, and layout.

```bash
./build/tools/cli/pulp inspect [script.js]
```

The inspector shows:
- View hierarchy with bounds, flex properties, and styles
- Widget state (values, labels, visibility)
- Theme tokens and computed colors
- Layout debug information

Use this when UI rendering doesn't match expectations or to understand the view tree structure.
