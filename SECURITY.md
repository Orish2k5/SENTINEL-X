# Security reporting and repository rules

This is a student engineering project, not a certified security product.

## Do not publish secrets

Never commit real credentials, tokens, private keys, HMAC keys, private-key certificates, `.env` files, production databases or logs containing sensitive values. Use synthetic examples only.

## If a secret is exposed

1. Revoke or rotate the exposed value immediately.
2. Tell the project lead and repository maintainers privately.
3. Check logs and deployment/configuration copies for further exposure.
4. Do not rely on deleting the file in a later commit; repository history may still contain it.

## Reporting a vulnerability

For this private student repository, contact the project lead directly through the team's agreed private channel. Do not include live credentials or sensitive device data in an issue.
