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
| 3 — PDV | Parcial | Quantidades fracionadas e impressão |
| 4 — Financeiro | Não implementada | Contas a pagar/receber e despesas |
| 5 — Dashboard/relatórios | Parcial | Indicadores financeiros, gráficos e exportação CSV/PDF |
| 6–9 | Não implementadas | Licenciamento, atualizações, nuvem e distribuição |

Backup local manual e restauração estão disponíveis; agendamento permanece pendente. Caixa e PDV agora possuem fluxo básico operacional, com limitações descritas abaixo.

## Próxima sequência

1. Completar fornecedores e campos necessários dos cadastros.
2. Completar estoque com inventário em lote e identificação do operador pelo futuro login.
3. Completar caixa/PDV com descontos e pagamentos divididos.
4. Histórico de vendas aceita filtro por período no formato `AAAA-MM-DD` e exportação CSV/PDF; permanecem pendentes indicadores financeiros.
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


## Continuação — autenticação offline

- Migração 6: usuários locais e vínculos autenticados em vendas, estoque, abertura/fechamento e movimentações de caixa; registros legados preservados com ID nulo.
- Primeiro administrador, login, logout com proteção de carrinho, gestão de contas (administrador/operador), redefinição e inativação. Autorizações no C++.
- PBKDF2-HMAC-SHA256 via OpenSSL, salt individual, 600.000 iterações; bloqueio persistido por cinco minutos após cinco falhas.
- Recuperação inicial por código aleatório de uso único, com rotação e hash persistido; sem senha mestra.
- Backup com usuários, restauração restrita ao esquema 6, sessão invalidada após restauração.
- OpenSSL Crypto é nova dependência. Windows e distribuição da biblioteca permanecem pendentes.
- Limites: dois perfis fixos, sem troca da própria senha na sessão, sem auditoria geral nem criptografia do arquivo SQLite. Consulte `docs/autenticacao.md`.

Validação: compilação concluída; **5/5 conjuntos de testes aprovados**. Após ampliar o fluxo de primeiro acesso, o teste de interface passou novamente, sem avisos QML. Telas de login e usuários conferidas em 960 × 640. Bancos temporários, sem alterações no banco real.


## Continuação — troca da própria senha

Data: 20/09/2026. Item “Troca da própria senha, exigindo a senha atual” da Etapa 1 em `ESTRUTURA_DO_PROJETO.md` marcado como concluído.

- Acesso pelo botão **Minha senha** na navegação, disponível para administrador e operador autenticados; exige senha atual, nova senha e confirmação.
- Validações no C++: sessão ativa, senhas coincidentes, política de 12–128 caracteres, senha atual correta e diferente da nova. Erros de senha atual incrementam o contador de tentativas; cinco falhas bloqueiam a alteração e o login por 300 segundos, e o sucesso zera o contador.
- Novo salt e hash PBKDF2 gravados em transação com verificação de sessão e versão; a gravação exige exatamente uma linha afetada.
- Sessão atual é mantida (o processo adota a nova versão); demais sessões são invalidadas pelo incremento de `session_version`. O código de recuperação existente permanece válido.
- Sem migração: banco e backups permanecem no esquema 6.
- Testes de serviço (`pos_test.cpp`, `ownPasswordChange`): sem sessão, senha curta, confirmação divergente, senha igual à atual, senha atual incorreta sem alterar hash/salt, sucesso com novo hash/salt e versão incrementada, sessão mantida e autorizada, bloqueio após cinco falhas, senha antiga rejeitada no login, recuperação pelo código original preservado e persistência após reabrir o banco.
- Teste de interface (`ui_test.cpp`): diálogo abre pelo botão, tentativa com senha atual errada mantém a sessão e exibe mensagem, alteração bem-sucedida encerra o diálogo e mantém a sessão, logout e novo login rejeitam a senha antiga e aceitam a nova. Captura gerada sem avisos QML.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**; diálogo conferido por captura em 960 × 640 (`password.png`). Testes em bancos temporários, sem tocar o banco real. Windows e GPU real permanecem pendentes.


## Continuação — código de recuperação para administradores adicionais

Data: 20/09/2026. Item “Emissão/rotação de código de recuperação para administradores adicionais” da Etapa 1 em `ESTRUTURA_DO_PROJETO.md` marcado como concluído.

- `Auth::issueRecoveryCode(id)` em `core/auth`: administrador autenticado gera código aleatório de 32 bytes para outra conta de administrador ativa. Somente o hash SHA-256 é persistido; o código é exibido uma vez ao emissor.
- Emissão em transação com exigência de exatamente uma linha afetada; substitui e invalida o código anterior da conta. Emitir para a própria conta, operador, conta inativa ou inexistente é rejeitado.
- Botão **Código de recuperação** em **Usuários**, ao editar outro administrador; código exibido com aviso de entrega por canal seguro e confirmação de guarda, como no primeiro acesso.
- A recuperação existente já aceitava qualquer administrador ativo com código válido; agora existem códigos para administradores adicionais e o comportamento está coberto por testes.
- Editar/redefinir a conta pela administração continua invalidando o código; uso do código na recuperação continua rotacionando-o e invalidando o anterior.
- Sem migração: banco e backups permanecem no esquema 6.
- Testes de serviço (`pos_test.cpp`, `recoveryCodeIssuance`): permissão de operador negada, alvos inválidos (inexistente, operador, própria conta, inativo), código com 64 caracteres e somente hash persistido, reemissão invalida o anterior, recuperação pelo código emitido com rotação, edição administrativa limpando o código, recuperação de conta inativa rejeitada, reativação e persistência após reabrir o banco.
- Teste de interface (`ui_test.cpp`): criação de segundo administrador, emissão pela tela **Usuários**, exibição e descarte do código, recuperação pela tela de login com o código emitido, login com a senha redefinida e descarte do código rotacionado. Capturas sem avisos QML.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**; telas conferidas por captura em 960 × 640 (`recovery-code.png`, `recovery-rotated.png`). Testes em bancos temporários, sem tocar o banco real. Windows e GPU real permanecem pendentes.


## Continuação — matriz de permissões por ação

Data: 20/09/2026. Item “Definir permissões por ação antes de oferecer perfis configuráveis” da Etapa 1 em `ESTRUTURA_DO_PROJETO.md` marcado como concluído.

- Inventário das verificações existentes: `read`, `catalog`, `inventory`, `cash`, `pos`, `settings`, `backup` e `users`, aplicadas em `core/auth`, `modules/catalog`, `modules/inventory`, `modules/pos`, `core/settings` e `infrastructure/backup`.
- Matriz centralizada em `Auth::permissions()` (`core/auth`): identificador estável por ação, descrição e concessão por perfil. `Auth::allowed` consulta a matriz; identificadores desconhecidos, perfis desconhecidos e sessões ausentes são negados.
- Comportamento preservado: administrador mantém todas as ações; operador mantém `read`, `cash` e `pos`. Nenhuma concessão nova ou removida.
- Sem migração e sem mudança de interface: banco e backups permanecem no esquema 6.
- Teste de serviço (`pos_test.cpp`, `permissionMatrix`): matriz completa com os oito identificadores e descrições, todas as ações concedidas ao administrador, concessões do operador conferidas item a item contra a matriz, identificadores desconhecidos/vazios/maiúsculos/injeção negados e tudo negado sem sessão.
- A matriz prepara a base para perfis configuráveis, que permanecem pendentes, agora com ponto único de definição.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**. Sem alteração de interface, os testes existentes de interface permanecem válidos. Testes em bancos temporários, sem tocar o banco real. Windows e GPU real permanecem pendentes.


## Continuação — auditoria das alterações de usuários e permissões

Data: 20/09/2026. Item “Registrar auditoria das alterações de usuários e permissões” da Etapa 1 em `ESTRUTURA_DO_PROJETO.md` marcado como concluído. **Etapa 1 completa.**

- Migração 7: tabela `audit_log` com usuário responsável (ID e nome da ocasião), ação, alvo, detalhes e data/hora local; índice por recentes.
- Novo módulo `core/audit` com `Audit::record` (gravação na transação do chamador) e leitor paginado de 50 registros com **Carregar mais**.
- Ações auditadas: `user.create`, `user.update`, `user.password`, `user.recovery_code` e `settings.update`. Detalhes reconstroem a alteração (nome, login, perfil, ativo, empresa, perfil de negócio e módulos) sem gravar senhas, hashes ou códigos.
- Falha na gravação da auditoria desfaz a alteração: `saveUser`, `changePassword`, `issueRecoveryCode` e `Settings::save` gravam dentro da própria transação e tratam falha como erro da operação. Na troca de senha, o registro é feito antes do incremento de versão para a sessão permanecer válida na gravação.
- Permissão nova `audit` (somente administrador) na matriz central; leitor e tela bloqueados para operador.
- Tela **Auditoria** no menu lateral (administrador), com responsável, ação, alvo, detalhes e horário local.
- Backup: restauração direta passa a exigir esquema 7; backups 6 são rejeitados sem conversão automática. `audit_log` integra o backup e a restauração transacional.
- Setup inicial e recuperação por código não são auditados: ocorrem sem sessão autenticada. Operações de venda/estoque/caixa já gravam o responsável em seus próprios registros.
- Testes de serviço (`pos_test.cpp`, `auditLog`): contagem e ordem das ações, alvo e detalhes, ausência de senhas nos registros, operação falhada sem auditoria, atualização sem redefinição de senha, paginação com 66 registros (50 + Carregar mais), leitura negada a operador e rollback quando a tabela de auditoria é removida (usuário e configuração inalterados). `permissionMatrix` atualizada para nove ações. Expectativas de versão dos testes de migração atualizadas para 7.
- Teste de interface (`ui_test.cpp`): tela **Auditoria** acessada pelo menu, quatro registros do fluxo administrativo (dois usuários criados, troca de senha, código emitido) na ordem correta, sem paginação pendente; três gravações de configurações conferidas no segundo fluxo. Captura sem avisos QML.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**; tela conferida por captura em 960 × 640 (`audit.png`). Testes em bancos temporários, sem tocar o banco real. Windows e GPU real permanecem pendentes.


## Continuação — fornecedores e vínculo com produtos

Data: 20/09/2026. Item “Cadastro de fornecedores e vínculos necessários” da Etapa 2 em `ESTRUTURA_DO_PROJETO.md` marcado como concluído.

- Migração 8: tabela `suppliers` (nome obrigatório, documento único quando informado, telefone, e-mail, ativo) e coluna `products.supplier_id` com vínculo opcional por FK.
- Módulo de cadastros estendido: seção **Fornecedores** com inclusão, edição, busca por nome/documento/telefone (com escapes), inativação/reativação e listagem no menu. Documento vazio é gravado como nulo, permitindo vários fornecedores sem documento.
- Produto ganha seletor **Fornecedor** opcional no formulário; fornecedores inativos deixam de ser oferecidos em novos vínculos, e vínculos existentes são preservados, como nas categorias. Vínculo com fornecedor inexistente é rejeitado pela FK.
- Permissões pela matriz existente: salvar/inativar exige `catalog`; consulta exige sessão.
- Backup: restauração direta passa a exigir esquema 8; backups 7 são rejeitados sem conversão automática.
- Fixture de testes (`auth_fixture.h`) rebaixa também fornecedores e a migração 8 nos testes de upgrade, mantendo-os válidos.
- Testes de serviço (`catalog_test.cpp`): `suppliersWorkflow` (nome vazio, documento duplicado, múltiplos sem documento, busca, edição, inativação/reativação, persistência após reabrir), `productSupplierLink` (vincular, trocar, desvincular, fornecedor inexistente rejeitado, vínculo preservado com fornecedor inativo) e `upgradeFromVersionSeven` (banco da versão anterior atualizado com produto preservado e vínculo funcional após a migração).
- Teste de interface (`ui_test.cpp`): criação de fornecedor pela tela **Fornecedores**, criação de produto com fornecedor selecionado no formulário e vínculo persistido. Capturas sem avisos QML.

Validação: compilação concluída e **5/5 conjuntos de testes aprovados**; telas conferidas por captura em 960 × 640 (`suppliers.png`, `catalog.png` com o seletor de fornecedor). Testes em bancos temporários, sem tocar o banco real. Windows e GPU real permanecem pendentes.


## Continuação — campos complementares (Etapa 2)

Entrega de 20/09/2026. A implementação parcial encontrada no banco, configurações e catálogo foi concluída com formulários, compatibilidade de backup e testes.

- Empresa: documento, telefone e endereço opcionais, com limites de tamanho e auditoria administrativa preservada.
- Cliente: endereço, nascimento e observações; valida data real/não futura e armazena ISO, exibindo DD/MM/AAAA.
- Produto: marca, unidade, máximo, localização e observações. Máximo vazio/zero significa não definido; positivo exige valor pelo menos igual ao mínimo. Unidade e máximo permanecem informativos.
- Migração 9 preserva dados e adiciona padrões vazios/zero. Restauração direta exige versão 9 e esquema idêntico. Backup 8 é rejeitado sem alterar dados atuais.
- Testes de campos opcionais, limites, datas inválidas, persistência, migração 8→9, idempotência, rollback de configurações em falha de auditoria, backup e formulários.

A auditoria geral dos cadastros é a próxima entrega. Campos específicos por segmento e venda fracionada continuam pendentes.

Validação: compilação concluída; **5/5 conjuntos de testes passaram**. Após incluir teste de máximo vazio e ampliar o fluxo de cliente, cadastros e interface passaram novamente (**2/2**), sem avisos QML. Telas conferidas em 960 × 640. Somente bancos temporários foram usados. Item marcado no roteiro; auditoria de cadastros permanece aberta.


## Continuação — auditoria dos cadastros (conclusão da Etapa 2)

Entrega de 20/09/2026:

- Criação, edição, inativação e reativação de produtos, categorias, clientes e fornecedores registradas em `audit_log` na mesma transação da alteração.
- Eventos identificam usuário autenticado, tipo e ID do cadastro, horário e nomes legíveis dos campos alterados. Valores pessoais e observações não são duplicados. Alterações de status registram ativo/inativo.
- Operações sem alteração efetiva não geram evento. Corrigida a comparação de vínculos nulos para evitar falso evento em produtos.
- Erro na auditoria desfaz criação, edição e mudança de status. Validações, registros inexistentes e acesso negado não geram evento de sucesso.
- Consulta existente continua restrita a administradores, com rótulos para os novos eventos. Não há reconstrução de histórico anterior.
- Sem migração: esquema e compatibilidade de backup permanecem na versão 9.

Validação: compilação concluída. Na execução completa, estoque, PDV, backup e interface passaram; o teste novo de cadastros detectou a comparação de nulos. Após a correção e melhoria dos rótulos, **cadastros e interface foram executados novamente e passaram (2/2)**. Os cinco conjuntos ficaram validados. Testes cobrem quatro tipos de cadastro, rollback, repetição sem mudanças, persistência, usuário, privacidade dos detalhes e bloqueio do operador. Interface conferida em 960 × 640, sem avisos QML; somente bancos temporários utilizados.

Item marcado [x] no roteiro e Etapa 2 concluída. Próxima entrega: descontos e acréscimos no PDV (Etapa 3).


## Continuação — descontos e acréscimos no PDV (Etapa 3)

- Ajustes em reais por venda, com duas casas decimais, justificativa de até 200 caracteres e total positivo dentro do limite existente.
- Permissão central `pos.adjust`, exclusiva de administrador nesta versão, verificada ao aplicar e finalizar.
- Subtotal preserva os itens; desconto e acréscimo compõem o total pago. Troco, caixa e indicadores usam o total final.
- Alterar itens remove os ajustes; falhas de finalização preservam o carrinho. Finalizar ou limpar remove ajustes e justificativa.
- Auditoria `sale.adjust` na mesma transação de venda, itens, pagamento e estoque; falha de auditoria desfaz toda a venda.
- Migração 10 preenche o subtotal histórico a partir do total e mantém ajustes antigos zerados. Restauração direta passa a exigir esquema idêntico e versão 10.
- UI de aplicação dos ajustes, resumo e detalhes persistidos. Percentuais, ajuste por item e rateio para devoluções permanecem fora desta entrega.
