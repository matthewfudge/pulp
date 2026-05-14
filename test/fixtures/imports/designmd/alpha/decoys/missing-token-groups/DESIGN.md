---
# Decoy fixture — filename matches, frontmatter fence is present,
# and `name:` is set, but there are zero canonical token-group keys
# (no colors, typography, rounded, spacing, components). The
# DESIGN.md detector MUST NOT match this file because the
# frontmatter-key any-of clause fails (all-of match mode).
name: Bare metadata file
version: alpha
author: someone
---

# Bare metadata file

This file has DESIGN.md-shaped metadata (name, version, author) but
no actual token vocabulary. The detector should reject it.
