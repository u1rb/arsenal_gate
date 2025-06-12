#!/bin/bash

# Demo script that sends multiple sample notifications

SERVER_URL="${SERVER_URL:-http://localhost:8000}"

echo "🚀 Sending demo notifications to $SERVER_URL"
echo ""

# Array of sample notifications
notifications=(
    "System Alert|Database backup completed successfully|low|system"
    "Security Warning|Failed login attempts detected|high|security"
    "Performance Alert|CPU usage above 80% for 5 minutes|medium|monitoring"
    "Deployment Status|Application v2.1.0 deployed successfully|low|deployment"
    "Error Alert|Payment processing service is down|high|critical"
    "Maintenance Notice|Scheduled maintenance starts in 1 hour|medium|maintenance"
)

# Send each notification
for notification in "${notifications[@]}"; do
    IFS='|' read -r title message priority category <<< "$notification"
    
    echo "Sending: $title ($priority)"
    
    curl -s -X POST "${SERVER_URL}/api/notifications" \
        -H "Content-Type: application/json" \
        -d "{
            \"title\": \"$title\",
            \"message\": \"$message\",
            \"priority\": \"$priority\",
            \"category\": \"$category\"
        }" > /dev/null
    
    if [ $? -eq 0 ]; then
        echo "✅ Sent successfully"
    else
        echo "❌ Failed to send"
    fi
    
    sleep 1
done

echo ""
echo "🎉 Demo notifications sent!"
echo "View them at: ${SERVER_URL}/"