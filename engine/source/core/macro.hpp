#define CNE_CHECK(X, P) \
    do { if (!(X)) { P("Check {3} failed in function '{1}', line: {0}, file: '{2}'", __LINE__, __FUNCTION__, __FILE__, #X); } } while (false)
