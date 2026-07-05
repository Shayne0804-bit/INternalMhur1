function notFoundHandler(req, res) {
  res.status(404).json({
    ok: false,
    error: 'not_found'
  });
}

function errorHandler(err, req, res, next) {
  if (res.headersSent) {
    return next(err);
  }

  const statusCode = err.statusCode || 500;
  return res.status(statusCode).json({
    ok: false,
    error: statusCode === 500 ? 'internal_error' : err.message
  });
}

module.exports = {
  notFoundHandler,
  errorHandler
};
