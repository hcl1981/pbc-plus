import json, os, subprocess, sys
g = json.load(open('geraete.json')); vor = open('prompt_vorlage.txt').read()
A = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '')   # pbcemu/art/
for n in sys.argv[2:]:
    geraet, details = g[n]
    open(f'prompt_{n}.txt', 'w').write(vor.replace('{GERAET}', geraet).replace('{DETAILS}', details))
    r = subprocess.run(['python3', '-I', 'gemini_bild.py', 'gemini-3-pro-image', '3:2', f'{n}_{sys.argv[1]}.png', f'prompt_{n}.txt',
                        A + 'zx.jpeg', A + 'sheet.jpeg', 'ref_7800_anordnung.png', f'ref_{n}.png'], capture_output=True, text=True)
    print(n, (r.stdout + r.stderr).strip().splitlines()[-1][:120])
