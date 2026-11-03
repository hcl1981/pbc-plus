"""Erzeugt ein Bild mit der Gemini-API.
   python3 gemini_bild.py <modell> <seitenverhältnis> <ausgabe.png> <prompt.txt> [referenz ...]"""
import base64, json, mimetypes, os, sys, urllib.request
modell, verh, aus, prompt_datei, *refs = sys.argv[1:]
key = next(l.split('=', 1)[1].strip() for l in open(os.path.expanduser('~/.config/gemini.env')) if l.startswith('GEMINI_API_KEY='))
teile = []
for r in refs:
    teile.append({'inline_data': {'mime_type': mimetypes.guess_type(r)[0] or 'image/png', 'data': base64.b64encode(open(r, 'rb').read()).decode()}})
teile.append({'text': open(prompt_datei, encoding='utf-8').read()})
body = {'contents': [{'parts': teile}], 'generationConfig': {'responseModalities': ['IMAGE', 'TEXT'], 'imageConfig': {'aspectRatio': verh}}}
req = urllib.request.Request(f'https://generativelanguage.googleapis.com/v1beta/models/{modell}:generateContent',
                             data=json.dumps(body).encode(), headers={'Content-Type': 'application/json', 'x-goog-api-key': key})
try:
    d = json.load(urllib.request.urlopen(req, timeout=300))
except urllib.error.HTTPError as e:
    sys.exit('HTTP %s: %s' % (e.code, e.read().decode()[:600]))
n = 0
for c in d.get('candidates', []):
    for p in c.get('content', {}).get('parts', []):
        if 'inlineData' in p or 'inline_data' in p:
            b = p.get('inlineData') or p.get('inline_data')
            open(aus, 'wb').write(base64.b64decode(b['data'])); n += 1
        elif 'text' in p:
            print('Text:', p['text'][:300])
print('Bilder:', n, '| Nutzung:', d.get('usageMetadata', {}))
if not n: print(json.dumps(d)[:800])
