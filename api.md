# Danisa API Reference

Danisa is a zero-knowledge relay service for end-to-end encrypted (E2EE) communications. This document specifies the REST and WebSocket API.

## Base URL
The API is prefixed with `/api/v1`.

## Authentication
Most endpoints require authentication using a session token. The token can be provided in two ways:
1.  **Header**: `Authorization: Bearer <token>`
2.  **Cookie**: `danisa_sid=<token>`

## User Management

### Check Username Availability
`GET /users/availability`

Checks if a username is available for registration.

**Query Parameters:**
* `u` (required): The username to check.

**Response (200 OK):**
```json
{
  "available": true
}
```

### Register User
`POST /users`

Creates a new user account.

**Request Body:**
```json
{
  "username": "alice",
  "password": "secure-password1"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "user_id": 123
}
```

### Get Current User Profile
`GET /users/me`

Retrieves the profile of the authenticated user.

**Response (200 OK):**
```json
{
  "username": "alice"
}
```

## Authentication

### Login
`POST /auth`

Authenticates a user and establishes a session.

**Request Body:**
```json
{
  "username": "alice",
  "password": "secure-password1"
}
```

**Response (200 OK):**
```json
{
  "status": "success",
  "token": "..."
}
```
*Note: Sets the `danisa_sid` cookie.*

### Logout
`DELETE /auth`

Invalidates the current session.

**Response (200 OK):**
```json
{
  "status": "success"
}
```

## Device Management

### Register Device
`POST /device/register`

Registers a device and its public key for the authenticated user.

**Request Body:**
```json
{
  "device_id": "uuid-or-unique-string",
  "public_key": "BASE64_ENCODED_PUBLIC_KEY"
}
```

**Response (201 Created):**
```json
{
  "status": "success"
}
```

**Response (400 Bad Request):**
```json
{
  "error": "maximum device limit reached"
}
```

### Get Device Public Key
`GET /device/key`

Retrieves the public key for a specific device.

**Query Parameters:**
* `device_id` (required): The ID of the device.

**Response (200 OK):**
```json
{
  "device_id": "...",
  "public_key": "BASE64_ENCODED_PUBLIC_KEY"
}
```

## Relay Service

### Send Relay Message
`POST /relay/send`

Enqueues an opaque encrypted packet for delivery to all devices of a target user.

**Request Body:**
```json
{
  "from_device_id": "your-device-id",
  "to_username": "recipient-username",
  "ciphertext": "BASE64_ENCODED_CIPHERTEXT",
  "timestamp": 1620000000
}
```

**Response (201 Created):**
```json
{
  "status": "success"
}
```

### Poll Messages
`GET /relay/poll`

Retrieves pending messages for the authenticated device.

**Query Parameters:**
* `device_id` (required): The ID of the authenticated device.

**Response (200 OK):**
```json
{
  "messages": [
    {
      "id": 101,
      "ciphertext": "..."
    }
  ]
}
```

### Acknowledge Message
`POST /relay/ack`

Acknowledges receipt of a message, allowing the server to remove it from the queue.

**Request Body:**
```json
{
  "device_id": "your-device-id",
  "message_id": 101
}
```

**Response (200 OK):**
```json
{
  "status": "success"
}
```

### WebSocket Relay
`GET /relay/ws`

Establishes a WebSocket connection for real-time message delivery.

**Query Parameters:**
* `device_id` (required): The ID of the authenticated device.

**Response:** `101 Switching Protocols` on success.

## System

### Health Check
`GET /health`

Checks the service status.

**Response (200 OK):**
```json
{
  "status": "success"
}
```

### Security Verify
`GET /security/verify`

Performs an integrity check on the security vault and database.

**Response (200 OK):**
```json
{
  "status": "success"
}
```
