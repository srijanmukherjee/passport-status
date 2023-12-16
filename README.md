# Passport Seva India Passport Status Query Tool

Extracts passport application status from [passportindia.gov.in](https://passportindia.gov.in) and outputs it in json format.

## :rocket: Running instruction
```shell
# build the program
make

# run
OPENSSL_CONF=openssl.conf ./build/status <fileno> <dob>
```

## Note

Without setting `UnsafeLegacyRenegotiation` option in `openssl.conf`, libcurl gives the following error

```shell
curl: (35) OpenSSL/3.1.4: error:0A000152:SSL routines::unsafe legacy renegotiation disabled
```

To learn more read this answer on stackoverflow: [https://stackoverflow.com/a/76012131](https://stackoverflow.com/a/76012131)

## :warning: NOTICE

This is for educational purpose only. Web scraping should be conducted with explicit permission from website administrators, adhering to legal and ethical standards to ensure responsible and lawful data extraction.