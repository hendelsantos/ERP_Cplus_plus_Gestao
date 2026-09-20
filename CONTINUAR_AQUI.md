# Continuidade — MH Store ERP

Este arquivo registra o ponto de parada após a implementação inicial de autenticação e permissões offline.

## Como retomar

Leia este arquivo, [ESTRUTURA_DO_PROJETO.md](ESTRUTURA_DO_PROJETO.md), `docs/progresso.md` e `MH_Store_ERP_Plano_Completo.md`. Confira os arquivos atuais antes de alterar: o plano descreve o produto desejado, não funcionalidades já concluídas.

Por orientação explícita do usuário, seguir `ESTRUTURA_DO_PROJETO.md` e atualizar suas caixas de seleção conforme as entregas forem concluídas e validadas. Não marcar entregas parciais como concluídas. Registrar evidências em `docs/progresso.md`.

O usuário vem autorizando a implementação por etapas com “continue”. Continue uma etapa funcional, valide e atualize a documentação. Não considere o ERP completo.

## Direção do produto

O usuário quer um software modular adaptável a diferentes negócios, para futura comercialização no Mercado Livre. Priorizar base comum e módulos com dependências explícitas. Não apresentar a versão atual como pronta para venda ou como suporte completo a todos os segmentos.

## Estado implementado

- Autenticação offline: setup inicial, administrador/operador, gestão de contas e recuperação por código. Leia `docs/autenticacao.md`.
- Registro central de módulos implementados, nomes, dependências e disponibilidade para menu/configurações.
- Empresa e perfil persistidos; Estoque/Caixa/PDV configuráveis, com dependências e bloqueios no C++.
- C++20, Qt 6.4+, QML, Qt SQL e SQLite local.
- Cadastros básicos de produtos, categorias e clientes: inclusão, edição, busca, inativação e reativação.
- Estoque: entrada, saída, ajuste por contagem, estoque crítico e histórico; bloqueia saldo negativo.
- Caixa: abertura, fechamento, suprimento, sangria, histórico e diferença entre esperado e contado.
- PDV: carrinho, quantidades inteiras, pagamento único em dinheiro/PIX/crédito/débito/outros, troco e resumo em tela.
- Venda, pagamento, itens e baixa de estoque na mesma transação.
- Consulta paginada de vendas e detalhes persistidos, com pesquisa por número.
- Dashboard real: faturamento diário/mensal, vendas, ticket médio, estoque crítico, sem estoque e saldo esperado da sessão aberta.
- Backup local manual e restauração em Configurações, com validação, confirmação e cópia anterior de segurança.
- Histórico de compras por cliente: acesso pelo cadastro, clientes inativos, paginação, total gasto, quantidade e última compra; somente vendas concluídas.
- Cliente opcional no PDV: clientes ativos, validação na finalização, vínculo persistido e exibição nos detalhes. Seleção volta a consumidor não identificado após venda ou limpeza do carrinho.

## Última validação

Após a inclusão de autenticação local:

- Compilação concluída.
- **5/5 conjuntos de testes passaram**: cadastros, estoque, caixa/PDV, backup e interface.
- Interface conferida em 960 × 640, incluindo login e administração de usuários.
- Os testes usam bancos temporários; não devem acessar os dados reais da aplicação.
- Windows e renderização em GPU real ainda não foram validados.

## Ambiente e comandos

Diretório de trabalho utilizado:

`/home/hendel/MH-DESENVOLVIMENTO-SOFTWARE/PDV_offline_First_C++`

Qt 6.4.2 preparado em `~/.local/share/mhstore-qt` por extração de pacotes Ubuntu, sem instalação administrativa. Use o script que configura bibliotecas/plugins:

```bash
./scripts/dev.sh build
./scripts/dev.sh test
./scripts/dev.sh run
```

Executável: `build/bin/MHStore`. `build/MHStore` é diretório do módulo QML, não executável.

Capturas opcionais dos testes de interface:

```bash
mkdir -p /tmp/mhstore-ui-review
MHSTORE_TEST_SCREENSHOTS=/tmp/mhstore-ui-review ./scripts/dev.sh test
```

O banco real é `mhstore.sqlite` no `QStandardPaths::AppDataLocation`, com organização `MHSoftware` e aplicação `MH Store`. A aplicação usa QLockFile para impedir duas instâncias no mesmo diretório de dados.

Repositório Git configurado para `https://github.com/hendelsantos/ERP_Cplus_plus_Gestao.git`, branch `main`. Compilação, bancos locais, backups e segredos são excluídos pelo `.gitignore`.

## Arquivos principais

- `desktop/src/core/database/database.cpp`: abertura e migrações SQLite.
- `desktop/src/modules/catalog/catalog.*`: cadastros.
- `desktop/src/modules/inventory/inventory.*`: estoque.
- `desktop/src/modules/pos/pos.*`: caixa, PDV, consultas de vendas, clientes disponíveis e dashboard.
- `desktop/src/infrastructure/backup/backup.*`: backup/restauração.
- `desktop/src/main.cpp`: inicialização, bloqueio de instância e objetos expostos ao QML.
- `desktop/qml/Main.qml`: navegação e dashboard.
- `desktop/qml/{CatalogPage,InventoryPage,PosPage,SalesPage,BackupPage}.qml`: telas.
- `desktop/tests/{catalog_test,inventory_test,pos_test,backup_test,ui_test}.cpp`: testes.
- `desktop/CMakeLists.txt`: aplicação e testes.
- `README.md`: instruções de uso.
- `docs/progresso.md`: comparação detalhada com o plano e evolução.

## Cuidados técnicos

- **Esquema atual: versão 6.** Usuários e vínculos autenticados em vendas, estoque, caixa e fechamento. Configurações de empresa permanecem na tabela `business_settings`.
- Restauração direta exige esquema idêntico e versão 6. Backups 4/5 são rejeitados; recuperação requer versão anterior em ambiente separado, seguida da atualização do banco. Não há conversão automática de arquivos antigos.
- `core/settings/settings.*`: registro central de módulos, navegação, configuração persistida e validação. A persistência mantém as colunas explícitas da versão 5; acrescentar módulos configuráveis exige revisar esquema e backup. PDV exige Estoque e Caixa. Alterar módulos exige caixa fechado e carrinho vazio. Perfil é descritivo; funcionalidades específicas por segmento ainda não existem.
- Módulos desabilitados bloqueiam operações no C++, preservando cadastros e histórico. Não são permissões ou licenciamento.
- Dinheiro do PDV/caixa usa centavos inteiros. Campos REAL legados permanecem como espelho para compatibilidade. Preserve a coerência ao alterar cadastros e vendas.
- Quantidades no estoque aceitam três casas decimais; no PDV, somente unidades inteiras, até 10.000 por item.
- Vendas e movimentações usam transações com bloqueio de escrita antes de verificar saldos. Não separar gravação da venda, pagamento e estoque.
- Saldo esperado do caixa = abertura + pagamentos em dinheiro + suprimentos − sangrias. PIX e cartões são registros manuais, sem integração bancária.
- Responsável vem da sessão; parâmetros antigos de nome são ignorados. ID do usuário é gravado nas operações, nomes antigos preservados. Não há auditoria geral ainda.
- Produto possui código/nome/preço preservados nos itens da venda. Cliente possui vínculo por ID, mas o nome exibido vem do cadastro atual; não existe snapshot histórico do nome do cliente.
- Backup `.mhb` é SQLite, sem criptografia ou anexos. Restauração exige caixa atual fechado e carrinho vazio, cria cópia anterior, restaura em transação e encerra o aplicativo após sucesso.
- A listagem de backups é uma lista de arquivos da pasta, não auditoria de operações. Caminhos são informados em campos de texto; não há seletor gráfico.
- Dashboard usa data local, somente vendas concluídas e produtos ativos. Atualiza ao entrar, a cada minuto visível e pelo botão.

## Próxima etapa sugerida — ainda não iniciada

**Próxima tarefa concreta: troca da própria senha**, detalhada em [ESTRUTURA_DO_PROJETO.md](ESTRUTURA_DO_PROJETO.md).

Sequência de evolução da base modular:

1. Evoluir autenticação: troca da própria senha, perfis/permissões configuráveis e recuperação para administradores adicionais.
2. Implementar auditoria geral de alterações de cadastros e usuários. Operações de venda/estoque/caixa já possuem vínculo autenticado.
3. Planejar recursos por segmento (grade, peso, serviços) antes de prometer suporte operacional.
4. Completar financeiro, fornecedores, comprovantes e distribuição Windows antes da versão comercial.

Consulte `docs/comercializacao.md` para os critérios de entrega. O filtro por período permanece pendente e foi adiado em favor da base modular.

## Outras pendências relevantes

- Fornecedores e campos complementares dos cadastros.
- Descontos/acréscimos, pagamentos divididos, quantidades fracionadas no PDV.
- Evolução de permissões e auditoria geral.
- Cancelamento/devolução com estorno controlado de estoque e pagamento.
- Filtro de vendas por período, comprovante impresso/exportado e relatórios CSV/PDF.
- Contas a pagar/receber e despesas.
- Backup automático, criptografia, compatibilidade com backups de versões anteriores e nuvem.
- Inventário em lote e custo médio.
- Licenciamento, atualização e instaladores.

Não há implementação em andamento a completar neste ponto: a etapa inicial de autenticação offline foi concluída e validada.

OpenSSL Crypto é dependência de compilação. Testes criam usuários reais em bancos temporários, sem bypass de autenticação no código de produção.
