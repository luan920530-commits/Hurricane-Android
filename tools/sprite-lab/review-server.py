#!/usr/bin/env python3
"""Serve the repository for the isolated sprite review; no game files are changed."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ReviewHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path in ('/', '/index.html'):
            self.send_response(302)
            self.send_header('Location', '/tools/sprite-lab/review.html')
            self.end_headers()
        else:
            # Serve only the public art and review files needed by this page,
            # not arbitrary repository files (especially .git).
            allowed = {
                '/tools/sprite-lab/review.html',
                '/tools/sprite-lab/sheets/run-study/README.md',
                '/tools/sprite-lab/sheets/run-study/REVISION.md',
                '/Hurrican/data/textures/static_jungle.png',
                '/Hurrican/data/textures/s_Jungle.png',
                '/Hurrican/data/textures/p1_hurri_laufen.png',
            }
            for operator in ('male', 'female'):
                allowed.add(f'/tools/sprite-lab/sheets/run-study/{operator}-run-4f-70x80.png')
                allowed.add(f'/tools/sprite-lab/sheets/run-study/candidates/{operator}-recovery-70x80.png')
                allowed.add(f'/tools/sprite-lab/sheets/run-study/candidates/{operator}-phase08-70x80.png')
            if self.path not in allowed:
                self.send_error(404)
                return
            super().do_GET()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8000)
    args = parser.parse_args()
    handler = partial(ReviewHandler, directory=str(ROOT))
    with ThreadingHTTPServer(('0.0.0.0', args.port), handler) as server:
        print(f'Sprite review listening on port {args.port}', flush=True)
        server.serve_forever()


if __name__ == '__main__':
    main()
