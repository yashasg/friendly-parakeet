---
configured: true
interval: 10
timeout: 30
description: "My squad work loop"
---

# Squad Work Loop

> ⚠️ Set `configured: true` in the frontmatter above to activate this loop.
> Run with: `squad loop`

## What to do each cycle

Describe what your squad should do every time the loop wakes up. Be specific —
the more context you give, the better your squad performs.

Examples:
- Check for new messages in a Teams channel and summarize action items
- Review recent pull requests and flag anything needing attention
- Run a health check on staging and report anomalies
- Scan the inbox for anything that needs a response today

go thru all the issues for this codebase on github and fix them by priority/relevent, if the issue is outdated close it. when you run out of issues, do a parallel pass with all the agents, identify and create issues on github and start the loop again.

During parallel passes for finding issues and when you are fixing issues your goals are:
- use existing API's from raylib, entt, use docs in @docs/entt and @docs/raylib_cheatsheet.md
- follow ECS architecture using entt.
- remember more code, more problems. We dont like problems in our code, so less code is better, we achieve less code by using API provided by raylib and ECS
- reduce code branching, prioritize early outs.
- apply SOLID principles.
- prioritize cleaning up code that can be deduped, and deleting files that dont have any references.
- delete outdated docs and clean up current docs, make it concise




## Monitoring (optional)

If you want your squad to watch external channels, enable monitor capabilities:

```bash
squad loop --monitor-email --monitor-teams
```

## Personality (optional)

If your squad has a specific voice or style, describe it here so each cycle
stays consistent.

Example: "Be concise. Use bullet points. Flag blockers clearly."

## Tips

- **Be specific.** Vague prompts produce vague results.
- **Set boundaries.** Tell the squad what NOT to do (e.g., "Don't send messages to anyone but me").
- **Start small.** Begin with one task per cycle, then expand.
- **Use frontmatter.** `interval` controls how often the loop runs. `timeout` caps each cycle.
