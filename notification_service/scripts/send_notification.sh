#!/bin/bash

# Demo script to send notifications using curl
# Usage: ./send_notification.sh [title] [message] [priority] [category]

# Default values
TITLE="${1:-System Alert}"
MESSAGE="${2:-This is a demo notification}"
PRIORITY="${3:-medium}"
CATEGORY="${4:-demo}"
SERVER_URL="${SERVER_URL:-http://localhost:8000}"

echo "Sending notification..."
echo "Title: $TITLE"
echo "Message: $MESSAGE"
echo "Priority: $PRIORITY"
echo "Category: $CATEGORY"
echo ""

# Send the notification
response=$(curl -s -X POST "${SERVER_URL}/api/notifications" \
  -H "Content-Type: application/json" \
  -d "{
    \"title\": \"$TITLE\",
    \"message\": \"$MESSAGE\",
    \"priority\": \"$PRIORITY\",
    \"category\": \"$CATEGORY\"
  }")

if [ $? -eq 0 ]; then
    echo "✅ Notification sent successfully!"
    echo "Response: $response"
else
    echo "❌ Failed to send notification"
    echo "Make sure the server is running on $SERVER_URL"
fi

echo ""
echo "View notifications at: ${SERVER_URL}/"