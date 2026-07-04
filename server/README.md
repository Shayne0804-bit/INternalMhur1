# RUGIR Auth Server

API Node.js pour verifier les cles premium avec MongoDB.

## Principe

- Les cles premium ne sont jamais stockees en clair.
- Le serveur stocke un hash HMAC de la cle.
- Le premier `hwid` valide est lie a la cle.
- Le DLL recoit un JWT court apres verification.
- Les secrets restent dans Railway via les variables d'environnement.

## Installation locale

```powershell
cd server
npm install
copy .env.example .env
npm run dev
```

Dans `.env`, mets ton `MONGODB_URI`. Si tu utilises Railway, utilise MongoDB Atlas ou une DB MongoDB accessible depuis Internet. MongoDB Compass est seulement l'application pour visualiser la DB, pas un serveur heberge pour Railway.

## Variables Railway

```text
MONGODB_URI=mongodb+srv://...
JWT_SECRET=long_secret_random
LICENSE_KEY_PEPPER=long_secret_random
ADMIN_TOKEN=long_secret_random
TOKEN_TTL=15m
```

Railway fournit `PORT` automatiquement.

## Creer une cle premium

```powershell
curl -X POST http://localhost:3000/api/admin/licenses `
  -H "Content-Type: application/json" `
  -H "x-admin-token: TON_ADMIN_TOKEN" `
  -d "{\"tier\":\"premium\",\"daysValid\":30,\"maxActivations\":1}"
```

La reponse retourne la cle en clair une seule fois. Garde-la pour ton client.

## Verification depuis le DLL

```http
POST /api/auth/verify
Content-Type: application/json

{
  "key": "RUGIR-ABCDE-FGHIJ-KLMNO-PQRST-UVWXY",
  "hwid": "hash-ou-id-machine",
  "version": "1.0.0"
}
```

Reponse OK :

```json
{
  "ok": true,
  "tier": "premium",
  "expiresAt": "2026-07-06T00:00:00.000Z",
  "token": "jwt_session_token"
}
```

## Routes

- `GET /health`
- `POST /api/auth/verify`
- `POST /api/admin/licenses`
- `GET /api/admin/licenses`
- `POST /api/admin/licenses/:id/revoke`
- `POST /api/admin/licenses/:id/reset-hwid`
