---
# Decoy fixture — filename matches and frontmatter fence is present,
# but the required `name:` key is missing. The DESIGN.md detector
# MUST NOT match this because the frontmatter-key required clause
# fails (all-of match mode).
colors:
  primary: "#000000"
typography:
  body-md:
    fontFamily: Inter
    fontSize: 16px
---

# Untitled (no `name:` key in frontmatter)
