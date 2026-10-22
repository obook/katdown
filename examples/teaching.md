# Teaching demo

## Callouts

> [!definition]
> A sequence where each term is the previous one plus a fixed number $r$:
> $u_{n+1} = u_n + r$.

> [!theorem]
> For an arithmetic sequence, $u_n = u_0 + n r$.

> [!proposition]
> The sum of the first $n+1$ terms is $\dfrac{(n+1)(u_0 + u_n)}{2}$.

> [!corollary]
> $1 + 2 + \dots + n = \dfrac{n(n+1)}{2}$.

> [!proof]
> By induction on $n$.

> [!method]
> Compute $u_{n+1} - u_n$ and check that it does not depend on $n$.

> [!remark]
> A constant sequence is arithmetic, with $r = 0$.

> [!exercise]
> $u_0 = 3$ and $r = 2$. Compute $u_{10}$.

> [!solution]
> $u_{10} = 3 + 10 \times 2 = 23$.

`lemma`, `axiom`, `claim`, `assumption`, `conjecture` and `hypothesis` work
too, and so do short names: `def`, `thm`, `lem`, `prop`, `cor`, `pf`,
`sol`... Each type also takes its French name, with or without accents:
`théorème`, `lemme`, `propriété`, `démonstration`, `exercice`, `correction`...

## Picture size

Pandoc's `{width=...}` after a picture, here `{width=30%}`:

![A small picture](image.png){width=30%}
