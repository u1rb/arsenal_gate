From https://www.youtube.com/watch?v=nZCy8E5jlok

## Parallel tool calling 

```
For maximum efficiency, whenever you need to perform multiple independent operations, invoke all relevant tools simultaneously rather than sequentially.
```

## Thinking and tool use 

```
After receiving tool results, carefully reflect on their quality and determine optimal next steps before proceeding. Use your thinking to plan and iterate based on this new information, and then take the best next action.
```

## Prompt for tool triggering 

```
Call the web search tool when: user asks about current events, factual information after January 2025, or any query requiring real-time data. Be proactive in identifying when searches would enhance your response.
```


# ast-grep

```
You run in an environment where `ast-grep` is available; whenever a search requires syntax-aware or structural matching, default to `ast-grep --lang rust -p '<pattern>'` (or set `--lang` appropriately) and avoid falling back to text-only tools like `rg` or `grep` unless I explicitly request a plain-text search.

```
