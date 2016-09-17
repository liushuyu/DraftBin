from octohub.connection import Connection
conn = Connection()
uri = '/repos/Kreogist/Mu/git/refs/tags'
resp = conn.send('GET', uri)
jsobj = resp.parsed
print(jsobj[-1]['ref'].split('/')[-1])

