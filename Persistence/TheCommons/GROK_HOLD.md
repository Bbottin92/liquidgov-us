# Holding — recommended order

1. **reqjson** — smallest real footgun. Daemon + `test_reqjson.sh`. No nginx.
2. **deploy scripts** — repo only.
3. **nginx `/guest`** — needs the deploy word.
4. **backup retention** — last.

Not a BUILD. Brandon names one.
