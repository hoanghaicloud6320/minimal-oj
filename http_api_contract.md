# HTTP API Contract

This document defines the API endpoints for the Minimal OJ server.

## Overview

All API requests are prefixed with `/api`. Responses are in JSON format.

## Endpoints

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| GET | `/api/health` | Check server health |
| GET | `/api/problems` | List all problems |
| POST | `/api/problems` | Create a new problem |
| GET | `/api/problems/{slug}` | Get details of a specific problem |
| PUT | `/api/problems/{slug}` | Update an existing problem |
| DELETE | `/api/problems/{slug}` | Delete a problem |
| POST | `/api/problems/{slug}/refresh` | Refresh problem test cases/config |
| POST | `/api/problems/{slug}/submit` | Submit a solution for a problem |
| GET | `/api/submissions/recent` | List recent submissions |

## API Details

### GET `/api/health`
Returns server health status.
**Response:** `{"ok": true, "service": "minimal-oj"}`

### GET `/api/problems`
Returns a list of problem summaries.

### POST `/api/problems`
Creates a new problem.

### GET `/api/problems/{slug}`
Returns details of the problem identified by `slug`.

### PUT `/api/problems/{slug}`
Updates the problem identified by `slug`.

### DELETE `/api/problems/{slug}`
Deletes the problem identified by `slug`.

### POST `/api/problems/{slug}/refresh`
Refreshes test cases and configurations for the problem.

### POST `/api/problems/{slug}/submit`
Submits source code for a problem.
**Request Body:**
```json
{
  "participant": "string (optional)",
  "source_code": "string"
}
```

### GET `/api/submissions/recent`
Lists recent submissions.
**Query Parameters:**
* `limit`: number of submissions (default 25).
