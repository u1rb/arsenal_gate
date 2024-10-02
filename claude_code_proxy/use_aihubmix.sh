export ANTHROPIC_BASE_URL=https://aihubmix.com
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"
# export ANTHROPIC_MODEL=claude-sonnet-4-20250514
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
export ANTHROPIC_SMALL_FAST_MODEL='claude-3-5-haiku-20241022'



bunx @anthropic-ai/claude-code --verbose -p "Hi What is CRTP in C++?"

