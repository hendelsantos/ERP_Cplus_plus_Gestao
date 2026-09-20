# Comparação com o plano e progresso

Revisão: 20/09/2026. Referência: `MH_Store_ERP_Plano_Completo.md`, especialmente seções 30–34 e 42.

## Situação encontrada

- CMake com C++20, Qt Quick/QML e Qt SQL.
- SQLite local com tabelas iniciais de categorias, produtos, clientes, caixa e vendas.
- Janela com navegação lateral; dashboard com valores fixos e demais módulos sem implementação.
- Sem cadastros operacionais, regras de estoque/venda/caixa ou testes.
- Na inspeção inicial, a pasta ainda não continha repositório Git.

## Implementado nesta continuação

- Cadastros básicos de produtos, categorias e clientes: inclusão, edição, busca e inativação/reativação.
- Formulários QML ligados à camada C++ e ao SQLite, com mensagens de erro.
- Produtos: código interno obrigatório e único, código de barras, nome, categoria, custo, preço de venda e estoque mínimo.
- Clientes: nome obrigatório, documento, telefone e e-mail opcionais.
- Categorias: nome obrigatório e único. Categorias inativas deixam de ser oferecidas em novos produtos; vínculos existentes são preservados.
- Busca por nome e identificador; produtos também por código/código de barras, clientes por documento/telefone. `%` e `_` são tratados como texto literal.
- Validação de valores não negativos e finitos; preços arredondados para duas casas. Formulários aceitam vírgula ou ponto decimal sem separador de milhar.
- Criação do esquema em transação e espera de até cinco segundos por bloqueios SQLite.
- Carregamento QML ajustado para o mínimo Qt 6.4 declarado no CMake.
- Teste Qt Test cobrindo persistência após reabertura, repetição da inicialização, duplicidades, categoria inválida, busca, edição e inativação.
- Dashboard identificado como demonstrativo.

## Comparação por fase

| Fase | Estado após esta etapa | Pendências principais |
| --- | --- | --- |
| 0 — Fundação | Parcial | Logging e tema reutilizável |
| 1 — Cadastros | Parcial | Usuários/login, fornecedores, campos complementares de produtos/clientes |
| 2 — Estoque | Parcial | Inventário em lote, custo médio, alertas no dashboard e identificação autenticada do operador |
| 3 — PDV | Parcial | Descontos/acréscimos, quantidades fracionadas, pagamento dividido, cancelamento e impressão |
| 4 — Financeiro | Não implementada | Contas a pagar/receber e despesas |
| 5 — Dashboard/relatórios | Parcial | Indicadores financeiros, gráficos e exportação |
| 6–9 | Não implementadas | Licenciamento, atualizações, nuvem e distribuição |

Backup local manual e restauração estão disponíveis; agendamento permanece pendente. Caixa e PDV agora possuem fluxo básico operacional, com limitações descritas abaixo.

## Próxima sequência

1. Completar fornecedores e campos necessários dos cadastros.
2. Completar estoque com inventário em lote e identificação do operador pelo futuro login.
3. Completar caixa/PDV com descontos e pagamentos divididos.
4. Adicionar agendamento de backup e ampliar relatórios.

O saldo de estoque não pode ser editado no cadastro. A autenticação e identificação do operador precisam ser consideradas antes de disponibilizar operações que exijam auditoria por usuário. A versão 3 migra preços, valores de venda e abertura/fechamento para centavos inteiros. O PDV usa esses campos; os campos REAL originais permanecem como espelho de compatibilidade.

## Continuação — Qt e estoque

- Qt 6.4.2 preparado em `~/.local/share/mhstore-qt` usando pacotes Ubuntu extraídos no diretório do usuário.
- Script `scripts/dev.sh` para configurar, compilar, testar e executar usando esse Qt local; configuração do CMake no VS Code atualizada.
- Executável movido para `build/bin/MHStore`, evitando colisão com o diretório do módulo QML `build/MHStore`.
- Migração versionada para a versão 2: tabela `inventory_movements` e índice por produto. Preserva cadastros e saldos existentes; rejeita bancos de versões futuras.
- Tela de estoque com pesquisa, filtro de saldo crítico, histórico por produto e movimentação manual.
- Entradas e saídas por quantidade; ajuste por contagem recebe o saldo final desejado. Quantidades com até três casas decimais.
- Saldo e histórico gravados em uma única transação, adquirindo o bloqueio de escrita antes de ler o saldo.
- Rejeita estoque negativo, produto inativo, quantidade inválida e ausência de motivo/responsável.
- Histórico com quantidade movimentada, saldo anterior/final, motivo, responsável manual e data/hora local; mostra até 200 registros recentes.
- Cadastro de produtos recarrega o saldo ao voltar da tela de estoque.

O responsável manual não equivale a uma identidade autenticada. Não há ainda inventário em lote, custo médio, permissões ou edição/exclusão de movimentações. Correções devem ser registradas como uma nova movimentação. Saldos que já existiam antes da migração são preservados, sem inventar histórico retroativo.

## Validação nesta máquina

- `./scripts/dev.sh build`: compilação concluída com Qt 6.4.2.
- `./scripts/dev.sh test`: **3/3 conjuntos passaram** — cadastros, estoque e interface.
- Estoque: entradas/saídas fracionárias, ajuste inclusive para zero, persistência, validações, migração de banco versão 1, repetição da inicialização e rejeição de versão futura.
- Falha simulada ao inserir histórico confirmou rollback do saldo; tentativa de saída acima do saldo não gerou movimentação.
- Interface: criação de categoria/produto, entrada de estoque pelo formulário, atualização do saldo no cadastro e consulta de histórico, sem avisos QML.
- Aplicação compilada abriu em modo offscreen com banco temporário, sem erros de carregamento. Capturas da interface foram conferidas em 960 × 640.
- Todos os testes usaram bancos temporários. Windows e renderização com GPU real ainda não foram validados.


## Continuação — caixa e PDV

- Migração 3 com preços em centavos, itens de venda, pagamentos e campos de conferência de caixa. A migração preserva registros existentes e impede mais de uma sessão aberta via índice único.
- Abertura e fechamento com responsável manual, saldo esperado, contado, diferença e histórico das últimas 100 sessões, com horário local.
- Carrinho com unidades inteiras, adição, remoção e pesquisa por nome/código/código de barras. Preço precisa ser positivo; máximo de 10.000 unidades por item.
- Uma forma de pagamento por venda: dinheiro, PIX, crédito, débito ou outros. Dinheiro exige recebido suficiente e calcula troco; as demais formas são registradas pelo valor exato da venda.
- Somente pagamentos em dinheiro entram no saldo esperado da gaveta; PIX/cartões não contam como dinheiro físico.
- Venda, itens com descrição/preço da ocasião, pagamento e movimentações de estoque gravados na mesma transação SQLite, com bloqueio de escrita antes das verificações.
- Revalidação de preço, produto ativo, estoque e sessão de caixa na finalização. Falha mantém o carrinho e desfaz todas as alterações de banco.
- Resumo da venda em tela após concluir; ainda sem impressão, consulta de vendas anteriores ou emissão fiscal.

Pendências: login/permissões, descontos/acréscimos, quantidades fracionadas no PDV, pagamentos divididos, cancelamento/devolução e impressão. O carrinho não sobrevive ao fechamento do aplicativo. Identificação do operador e registro de PIX/cartão são manuais. Saldos históricos migrados sem pagamentos detalhados não recebem um histórico de recebimentos inventado; sessões antigas fechadas sem valor esperado não exibem diferença.

### Validação desta continuação

- Compilação com Qt 6.4.2.
- `./scripts/dev.sh test`: **4/4 conjuntos passaram** — cadastros, estoque/migrações, caixa/PDV e interface.
- Venda em dinheiro com troco, venda em PIX, fechamento com diferença, persistência, caixa duplicado/fechado, preço alterado, produto inativo e estoque insuficiente.
- Falha de pagamento simulada após inserir itens e baixar estoque: todas as alterações foram desfeitas. Falta de saldo em um segundo produto também desfez a baixa do primeiro.
- Migração de preços anteriores testada com R$ 19,99 e R$ 10,01, convertidos para 1999 e 1001 centavos.
- Fluxo pela interface: cadastro → entrada de estoque → abertura → venda → fechamento, com bancos temporários e sem avisos QML. Capturas de PDV e caixa conferidas em 960 × 640.


## Continuação — suprimento e sangria

- Migração 4 adiciona `cash_movements`, com vínculo à sessão, tipo, valor em centavos, saldos anterior/final, motivo, responsável e data.
- Suprimento adiciona dinheiro físico; sangria retira. O mesmo cálculo de saldo é usado na tela, na validação de retirada e no fechamento: abertura + pagamentos em dinheiro + suprimentos − sangrias.
- Operação adquire bloqueio de escrita antes de conferir a sessão e o saldo. Uma tela desatualizada não permite retirar dinheiro acima do saldo atual.
- Formulário exige valor positivo com até duas casas decimais, motivo e responsável. Sessão inexistente ou fechada é rejeitada.
- Histórico por sessão mostra até 200 registros manuais com horário local, inclusive após fechamento ou reabertura do aplicativo. Totais do caixa consideram todos os registros, sem esse limite de apresentação.
- Registros não são editados/excluídos pela interface. Responsável ainda é informado manualmente, sem login/permissões.

Validação adicionada: composição do saldo com dinheiro/PIX e movimentações, fechamento com diferença, persistência, histórico separado por sessão, limites/valores inválidos, saldo desatualizado, sessão fechada, falha simulada na gravação e migração de banco versão 3 com venda existente. O teste da interface inclui suprimento, sangria, fechamento e consulta do histórico.

Resultado: `./scripts/dev.sh build` concluído e `./scripts/dev.sh test` com **4/4 conjuntos aprovados**, sem avisos QML. Histórico conferido visualmente em 960 × 640. Testes executados em bancos temporários.


## Continuação — consulta de vendas

- Nova tela **Vendas** com pesquisa por número exato, paginação de 50 registros e ordenação do mais recente para o mais antigo.
- Detalhes persistidos: data/hora local, sessão de caixa, responsável, status, itens, preços da ocasião, total, pagamento, recebido e troco.
- Leitura dos itens a partir de `sale_items`, preservando código, nome e preço da venda após alterações no cadastro. Nenhuma migração adicional necessária.
- Registros antigos sem itens ou pagamento são apresentados como dados indisponíveis. Falhas ao abrir outra venda limpam os detalhes anteriores.
- Testes adicionados para reabertura do banco, alterações no produto após vender, pesquisa inválida, registro ausente, paginação e registros antigos sem detalhamento. O teste de interface navega até **Vendas**, pesquisa e abre os detalhes.

Permanecem pendentes filtro por período, impressão/exportação, cancelamento e devolução.

Validação da consulta: compilação concluída, **4/4 conjuntos de testes passaram**, sem avisos QML. Tela de detalhes conferida em 960 × 640.


## Continuação — dashboard com dados reais

- Substituídos os cards demonstrativos por consultas reais de faturamento diário/mensal, número de vendas, ticket médio diário, estoque crítico e dinheiro esperado no caixa aberto.
- Apenas vendas concluídas entram nos indicadores; períodos usam a data local do computador. Valores monetários vêm dos campos em centavos.
- Estoque crítico e produtos sem estoque consideram somente produtos ativos. Saldo de caixa usa abertura, pagamentos em dinheiro, suprimentos e sangrias.
- Indicador fictício de contas vencidas removido; financeiro permanece pendente.
- Atualização ao iniciar, ao retornar ao painel, pelo botão e a cada minuto enquanto visível. Todos os indicadores são lidos na mesma consulta SQLite. Erro de consulta limpa os valores e mostra indisponibilidade.
- Testes cobrem banco sem vendas, datas de mês anterior, exclusão de vendas canceladas, produtos inativos, caixa com movimentações, fechamento e falha de consulta. Teste da interface confere os indicadores após venda e fechamento.

Validação do dashboard: compilação concluída, **4/4 conjuntos de testes aprovados**, sem avisos QML; painel conferido visualmente em 960 × 640.


## Continuação — backup local e restauração

- Serviço `infrastructure/backup` e tela em **Configurações**, com pasta por caminho absoluto, criação manual e listagem dos arquivos `.mhb` presentes.
- Snapshot consistente via SQLite `VACUUM INTO`; integridade e chaves estrangeiras verificadas. O arquivo contém somente o banco SQLite, sem criptografia ou anexos externos.
- Restauração exige arquivo com esquema idêntico e migração 4. O arquivo é inspecionado em conexão somente leitura e copiado para área temporária antes da restauração.
- Bloqueio de restauração com caixa atual aberto ou carrinho pendente. A confirmação identifica o arquivo e informa a substituição dos dados e encerramento do aplicativo.
- Antes da alteração, cria backup `antes_restauracao_*.mhb` na pasta de dados/backups. Dados e sequências de identificadores são restaurados em uma transação, com verificação final dos vínculos e rollback em falhas.
- Após sucesso, encerra o aplicativo para descartar caches antigos. Ao abrir novamente, o estado recuperado é carregado. Uma sessão aberta no backup também é recuperada.
- Bloqueio de segunda instância via QLockFile. A listagem de arquivos não substitui uma auditoria de operações.
- Pendentes: agendamento, seletor gráfico de pastas, compressão/criptografia, configurações externas, migração de backups antigos e nuvem.

Validação: **5/5 conjuntos de testes aprovados** e compilação concluída. Cobertura de snapshot em WAL, restauração, reabertura, sequências, recuperação pela cópia anterior, arquivo inválido, esquema incompatível, vínculos quebrados, caixa/carrinho pendentes, caminhos inválidos e falha simulada de inserção com rollback. Interface testada criando backup e cancelando a confirmação de restauração, sem avisos QML; tela conferida em 960 × 640. Todos os testes utilizaram bancos temporários.


## Continuação — cliente opcional no PDV

- Seleção de clientes ativos no PDV, com nome e código para distinguir homônimos. Venda sem identificação continua disponível.
- Cliente revalidado na transação de finalização; cliente inexistente/inativo não gera venda nem baixa de estoque.
- Vínculo persistido no campo `sales.customer_id` existente. Resumo imediato inclui o nome; detalhes da venda consultam o nome atual e o identificador, inclusive se o cadastro estiver inativo.
- Após concluir venda ou limpar carrinho, seleção volta a consumidor não identificado. Navegação preserva a seleção e indica cliente indisponível se ele sair da lista.
- Sem nova migração: mantém compatibilidade com os backups de esquema 4. Nome histórico do cliente e histórico agregado de compras permanecem pendentes.
- Testes adicionados para clientes inexistentes/inativos, mudança de status antes de finalizar, venda anônima, persistência e nome atualizado. Fluxo de interface seleciona cliente pelo teclado, vende e confere vínculo e limpeza da seleção.

Validação de cliente opcional: compilação concluída e **5/5 conjuntos de testes aprovados**, incluindo seleção pela interface e consulta do vínculo persistido. Ponto de retomada registrado em `CONTINUAR_AQUI.md`.


## Continuação — histórico de compras por cliente

- Acesso pelo botão Compras em Clientes, inclusive em cadastros inativos.
- Reutiliza listagem paginada e detalhes de venda, com busca por número dentro do cliente.
- Total gasto em centavos, quantidade e última compra calculados sobre todas as vendas concluídas do cliente, independentemente da página e da busca.
- Exclui vendas anônimas e canceladas; apresenta zero e nenhuma compra para clientes sem histórico concluído.
- Retorno ao cadastro e opção Todas as vendas para limpar o filtro.
- Sem migração: esquema e backups permanecem na versão 4.
- Testes adicionados para paginação, agregados, persistência, cliente inativo/sem compras/inexistente, isolamento de vendas anônimas, falhas de consulta e navegação pela interface.


## Continuação — empresa e módulos configuráveis

- Configurações → Empresa e módulos: nome, perfil e Estoque/Caixa/PDV habilitados.
- Perfil descritivo; recursos específicos de roupas, mercados e serviços continuam pendentes.
- PDV depende de Estoque e Caixa, com validação no serviço e restrição no banco.
- Alterações de módulos bloqueadas com caixa aberto ou carrinho pendente; dados preservados ao desabilitar.
- Operações de PDV, caixa e movimentação manual de estoque verificam habilitação no C++.
- Menu oferece recursos implementados; removidos atalhos de fornecedores, financeiro e relatórios ainda vazios.
- Migração 5 e configurações incluídas no backup. Restauração direta apenas com esquema idêntico na versão 5; backups antigos rejeitados sem modificar o banco atual.
- Testes de configuração, persistência, dependências, migração, bloqueios operacionais, backup e interface.

Validação desta etapa: compilação concluída e **5/5 conjuntos de testes aprovados**, sem avisos QML; tela conferida em 960 × 640. Testes em bancos temporários. Windows permanece pendente.


## Continuação — registro central de módulos

- Identificadores, nomes, ordem de navegação, disponibilidade para configuração e dependências definidos em `core/settings/settings.cpp`.
- Menu e caixas de seleção gerados pelo registro, com descrição das dependências nas dicas da interface.
- API de seleção por identificador valida conjunto completo, valores booleanos e ausência de módulos desconhecidos antes de salvar.
- Verificação operacional do PDV exige seus módulos dependentes habilitados, inclusive diante de configuração externa inconsistente.
- Sem migração: banco e backups permanecem no esquema 5. A persistência ainda usa colunas explícitas; novos módulos configuráveis exigem revisar persistência, migração e backup.
- Usuários, autenticação, permissões e auditoria autenticada são a próxima etapa; não foram implementados nesta entrega.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**, incluindo registro, dependências, seleção inválida e navegação. Interface sem avisos QML, conferida em 960 × 640.


## Versionamento

Repositório Git iniciado na branch `main`, com código, testes, scripts e documentação. Arquivos de compilação, bancos, backups e configurações secretas são ignorados. Remoto: `https://github.com/hendelsantos/ERP_Cplus_plus_Gestao.git`.
