const cors = require('cors');
const express = require('express');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const config = require('./config/env');
const { connectDatabase } = require('./config/database');
const authRoutes = require('./routes/auth');
const adminRoutes = require('./routes/admin');
const updateRoutes = require('./routes/update');
const { errorHandler, notFoundHandler } = require('./middleware/errorHandler');

const app = express();

app.set('trust proxy', 1);
app.use(helmet());
app.use(express.json({ limit: '32kb' }));
app.use(rateLimit({
  windowMs: 60 * 1000,
  max: 120,
  standardHeaders: true,
  legacyHeaders: false
}));

if (config.allowedOrigins.length > 0) {
  app.use(cors({
    origin(origin, callback) {
      if (!origin || config.allowedOrigins.includes(origin)) {
        return callback(null, true);
      }

      return callback(new Error('cors_origin_denied'));
    }
  }));
}

app.get('/', (req, res) => {
  res.json({
    ok: true,
    service: 'rugir-auth-server'
  });
});

app.get('/health', (req, res) => {
  res.json({
    ok: true,
    uptime: process.uptime()
  });
});

app.use('/api/auth', authRoutes);
app.use('/api/admin', adminRoutes);
app.use('/api/update', updateRoutes);
app.use(notFoundHandler);
app.use(errorHandler);

async function start() {
  await connectDatabase(config.mongoUri);
  app.listen(config.port, () => {
    console.log(`Auth server listening on port ${config.port}`);
  });

  // Discord bot runs in the same process (same Railway service). A bot failure
  // must not take the auth API down.
  const { startBot } = require('./bot');
  startBot().catch((err) => console.error('[bot] startup failed:', err.message));
}

start().catch((err) => {
  console.error('Failed to start auth server:', err.message);
  process.exit(1);
});
