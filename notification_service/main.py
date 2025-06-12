from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime, timedelta
import uuid
import aiosqlite
import asyncio
import json

app = FastAPI(title="Notification Service")

DATABASE_PATH = "notifications.db"

class NotificationCreate(BaseModel):
    title: str
    message: str
    priority: str = "medium"  # low, medium, high
    category: Optional[str] = None

class Notification(BaseModel):
    id: str
    title: str
    message: str
    priority: str
    category: Optional[str]
    created_at: datetime
    resolved: bool = False
    resolved_at: Optional[datetime] = None

templates = Jinja2Templates(directory="templates")

async def init_database():
    """Initialize the SQLite database"""
    async with aiosqlite.connect(DATABASE_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS notifications (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                message TEXT NOT NULL,
                priority TEXT NOT NULL,
                category TEXT,
                created_at TEXT NOT NULL,
                resolved INTEGER DEFAULT 0,
                resolved_at TEXT
            )
        """)
        await db.commit()

async def cleanup_resolved_notifications():
    """Delete resolved notifications older than 10 seconds"""
    while True:
        try:
            cutoff_time = datetime.now() - timedelta(seconds=10)
            async with aiosqlite.connect(DATABASE_PATH) as db:
                await db.execute("""
                    DELETE FROM notifications 
                    WHERE resolved = 1 AND resolved_at < ?
                """, (cutoff_time.isoformat(),))
                await db.commit()
        except Exception as e:
            print(f"Error in cleanup task: {e}")
        await asyncio.sleep(5)  # Check every 5 seconds

@app.on_event("startup")
async def startup_event():
    """Initialize database and start cleanup task"""
    await init_database()
    asyncio.create_task(cleanup_resolved_notifications())

@app.post("/api/notifications", response_model=Notification)
async def create_notification(notification: NotificationCreate):
    """Create a new notification"""
    new_notification = Notification(
        id=str(uuid.uuid4()),
        title=notification.title,
        message=notification.message,
        priority=notification.priority,
        category=notification.category,
        created_at=datetime.now()
    )
    
    async with aiosqlite.connect(DATABASE_PATH) as db:
        await db.execute("""
            INSERT INTO notifications (id, title, message, priority, category, created_at)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (
            new_notification.id,
            new_notification.title,
            new_notification.message,
            new_notification.priority,
            new_notification.category,
            new_notification.created_at.isoformat()
        ))
        await db.commit()
    
    return new_notification

@app.get("/api/notifications", response_model=List[Notification])
async def get_notifications(resolved: Optional[bool] = None):
    """Get all notifications, optionally filter by resolved status"""
    async with aiosqlite.connect(DATABASE_PATH) as db:
        if resolved is None:
            cursor = await db.execute("SELECT * FROM notifications ORDER BY created_at DESC")
        else:
            cursor = await db.execute(
                "SELECT * FROM notifications WHERE resolved = ? ORDER BY created_at DESC",
                (1 if resolved else 0,)
            )
        
        rows = await cursor.fetchall()
        notifications = []
        
        for row in rows:
            notification = Notification(
                id=row[0],
                title=row[1],
                message=row[2],
                priority=row[3],
                category=row[4],
                created_at=datetime.fromisoformat(row[5]),
                resolved=bool(row[6]),
                resolved_at=datetime.fromisoformat(row[7]) if row[7] else None
            )
            notifications.append(notification)
        
        return notifications

@app.put("/api/notifications/{notification_id}/resolve")
async def resolve_notification(notification_id: str):
    """Mark a notification as resolved"""
    resolved_at = datetime.now()
    
    async with aiosqlite.connect(DATABASE_PATH) as db:
        cursor = await db.execute(
            "UPDATE notifications SET resolved = 1, resolved_at = ? WHERE id = ?",
            (resolved_at.isoformat(), notification_id)
        )
        await db.commit()
        
        if cursor.rowcount == 0:
            raise HTTPException(status_code=404, detail="Notification not found")
        
        return {"message": "Notification resolved"}

@app.get("/", response_class=HTMLResponse)
async def dashboard(request: Request):
    """Main dashboard page"""
    return templates.TemplateResponse("dashboard.html", {"request": request})

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
