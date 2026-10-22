# Teaching demo

## Callouts

> [!definition] Arithmetic sequence
> A sequence where each term is the previous one plus a fixed number $r$:
> $u_{n+1} = u_n + r$.

> [!theorem]
> For an arithmetic sequence, $u_n = u_0 + n r$.

> [!property]
> The sum of the first $n+1$ terms is $\dfrac{(n+1)(u_0 + u_n)}{2}$.

> [!proof]
> By induction on $n$.

> [!method] Showing a sequence is arithmetic
> Compute $u_{n+1} - u_n$ and check that it does not depend on $n$.

> [!remark]
> A constant sequence is arithmetic, with $r = 0$.

> [!exercise]
> $u_0 = 3$ and $r = 2$. Compute $u_{10}$.

> [!solution]- Solution
> $u_{10} = 3 + 10 \times 2 = 23$.

## French names

Each type also takes its French name, with or without accents.

> [!théorème] Théorème de Pythagore
> Dans un triangle rectangle, $a^2 + b^2 = c^2$.

> [!correction]- Correction
> Le repli marche comme pour les autres types.

## Picture size

Pandoc's `{width=...}` after a picture, here `{width=30%}`:

![A small picture](image.png){width=30%}
