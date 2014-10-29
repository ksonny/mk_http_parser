# (Experimental) Monkey HTTP Parser

This is an ongoing work to replace the current HTTP parser in Monkey HTTP Server. The server is really optimized and now the HTTP parsers needs an upgrade.

The following code aims to work from the following perspective:

- This parser is for HTTP/1.1
- It allows 3 states for a parsed request:
  - MK\_HTTP\_OK: it's OK, can be processed.
  - MK\_HTTP\_PENDING: there is some missing bytes, try later.
  - MK\_HTTP\_ERROR: something wrong in the request.
- The parser can be executed as many times over a request context, it will use some offsets to avoid re-parsing previous text.
- The parser do not care about logic based on protocol specs, mostly grammar for the first row, headers and optional body.
- Avoid contexts switches as much as possible.
- Do not mess current Monkey internal structures (yet).

## notes

This code is an experiment.
