# Estrutura e roteiro — MH Store ERP

Referência de trabalho para as próximas etapas. Atualizado em 20/09/2026.

## Regra de acompanhamento

Por orientação do usuário, este roteiro é a referência principal para as próximas entregas.

- Seguir a sequência definida, salvo nova orientação do usuário.
- Manter `[ ]` enquanto houver implementação ou validação pendente.
- Marcar `[x]` somente após concluir o escopo e as verificações aplicáveis.
- Ao marcar um item, registrar a data e a evidência de validação em `docs/progresso.md`.
- Entregas parciais devem ser descritas como parciais, mantendo o item aberto.
- Atualizar `CONTINUAR_AQUI.md` com o ponto de parada e a próxima tarefa.
- Ao finalizar cada etapa, informar ao usuário os itens concluídos e o próximo passo.

## 1. Objetivo do produto

Construir um ERP desktop modular, offline-first, adaptável a diferentes negócios, para futura comercialização no Mercado Livre. A base comum será compartilhada; cada segmento receberá recursos próprios conforme sua necessidade.

A versão atual está em desenvolvimento. Selecionar um perfil de negócio ainda não implementa funcionalidades específicas daquele segmento. A versão comercial inicial terá um escopo explícito e validado; a expansão para outros negócios será gradual.

## 2. Documentos e responsabilidades

| Arquivo | Finalidade |
| --- | --- |
| **Este arquivo** | Estrutura, prioridades e critérios para guiar o desenvolvimento |
| [CONTINUAR_AQUI.md](CONTINUAR_AQUI.md) | Ponto de parada, ambiente e cuidados para retomar |
| [README.md](README.md) | Compilar, executar e usar as funcionalidades existentes |
| [docs/progresso.md](docs/progresso.md) | Histórico das entregas e validações |
| [docs/autenticacao.md](docs/autenticacao.md) | Funcionamento e limites do acesso local |
| [docs/comercializacao.md](docs/comercializacao.md) | Critérios de preparação e liberação comercial |
| [Plano completo](MH_Store_ERP_Plano_Completo.md) | Visão ampla do produto desejado; não representa recursos concluídos |

Ao iniciar uma etapa, conferir o código e o ponto de continuidade. Ao concluir, atualizar os documentos afetados e o estado do roteiro.

## 3. Princípios de arquitetura

- Operações essenciais, usuários, permissões e dados funcionam localmente, sem internet.
- A interface QML apresenta dados e solicita ações; validações, permissões e regras ficam no C++.
- SQLite é a fonte dos dados locais. Alterações de esquema usam migrações versionadas.
- Valores de PDV e caixa usam centavos inteiros. Venda, pagamento e estoque são gravados na mesma transação.
- Módulos declaram dependências. Desabilitar um módulo preserva seus dados e impede operações protegidas.
- Perfil de negócio, módulo habilitado, permissão de usuário e licença comercial são conceitos separados.
- Serviços online serão complementares. Qualquer política futura de licença deve definir como preservar a operação offline.
- A estrutura será ampliada conforme as funcionalidades exigirem. A separação de responsabilidades deve acompanhar entregas funcionais e verificadas.

## 4. Estrutura existente

```text
PDV_offline_First_C++/
├── desktop/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp                  # Inicialização e objetos expostos ao QML
│   │   ├── core/
│   │   │   ├── auth/                 # Login, usuários, perfis e recuperação
│   │   │   ├── audit/                # Auditoria de alterações administrativas
│   │   │   ├── database/             # SQLite e migrações
│   │   │   └── settings/             # Empresa, perfis e registro de módulos
│   │   ├── modules/
│   │   │   ├── catalog/              # Produtos, categorias e clientes
│   │   │   ├── inventory/            # Saldo e movimentações de estoque
│   │   │   └── pos/                  # PDV, caixa, consultas de vendas e dashboard
│   │   └── infrastructure/
│   │       └── backup/               # Backup e restauração local
│   ├── qml/                         # Telas e navegação
│   └── tests/                       # Testes de serviços e interface
├── scripts/dev.sh                   # Compilar, testar e executar nesta máquina
├── docs/                            # Progresso, autenticação e comercialização
├── CONTINUAR_AQUI.md
├── ESTRUTURA_DO_PROJETO.md
├── MH_Store_ERP_Plano_Completo.md
└── README.md
```

Tecnologias atuais: C++20, Qt 6.4+, Qt Quick/QML, Qt SQL, SQLite, OpenSSL Crypto e CMake. Banco atual: **esquema 7**. Restauração direta exige esquema idêntico e versão 7.

## 5. Estado dos módulos

“Base disponível” indica um fluxo implementado, com limitações documentadas; não significa módulo comercial completo.

| Área | Estado atual | Evolução necessária |
| --- | --- | --- |
| Empresa e módulos | Base disponível | Ampliar configurações conforme os novos módulos |
| Usuários e acesso offline | Base disponível: administrador/operador, troca da própria senha e recuperação para todos os administradores | Permissões configuráveis |
| Produtos, categorias e clientes | Base disponível | Campos complementares e requisitos por segmento |
| Fornecedores | Pendente | Cadastro, busca, status e vínculos com produtos/compras |
| Estoque | Base disponível | Inventário em lote e custo médio |
| PDV | Base disponível | Desconto/acréscimo, pagamento dividido e quantidade fracionada |
| Caixa | Base disponível | Ampliar conferência e relatórios |
| Histórico de vendas/clientes | Base disponível | Período, exportações e comprovantes |
| Cancelamento e devolução | Pendente | Permissões, justificativa e estorno transacional |
| Financeiro | Pendente | Contas a pagar/receber, despesas e baixas |
| Dashboard | Indicadores básicos disponíveis | Novos indicadores e relatórios conforme dados existentes |
| Backup local | Manual e restauração disponíveis | Agendamento e compatibilidade com versões anteriores |
| Auditoria administrativa | Base disponível: usuários, senhas, códigos e configurações | Estender a cadastros e operações sensíveis (Etapa 2) |
| Licença, atualização e instalador | Pendentes | Distribuição e validação comercial em Windows |

As operações de venda, estoque e caixa já gravam o usuário autenticado. Isso ainda não constitui auditoria geral de todas as alterações.

## 6. Dependências e extensões

| Componente | Dependências ou regra |
| --- | --- |
| PDV atual | Estoque e Caixa habilitados, usuário autorizado e sessão de caixa aberta para finalizar |
| Estoque manual | Cadastro de produto, módulo habilitado e administrador |
| Caixa | Módulo habilitado e usuário autorizado |
| Usuários, configurações e backup | Administrador |
| Consulta da auditoria administrativa | Administrador |
| Consultas operacionais | Usuário autenticado; ambos os perfis atuais podem consultar |
| Alterar módulos | Caixa fechado e carrinho vazio |
| Restaurar backup | Administrador, caixa fechado, carrinho vazio e backup compatível |

Para introduzir um módulo, definir: finalidade, identificador, dependências, permissões, dados, migração, impacto em backup, telas e testes. O registro atual fica em `desktop/src/core/settings/settings.cpp`; a persistência ainda usa colunas explícitas, portanto novos módulos configuráveis podem exigir migração.

Separações futuras, quando a implementação justificar:

- `modules/cash/`: extrair regras de caixa hoje presentes em `pos`.
- `modules/sales/`: consultas, cancelamentos e devoluções.
- `modules/suppliers/`: fornecedores.
- `modules/finance/`: contas, despesas e baixas.
- `modules/reports/`: consultas de relatórios e exportações.
- `core/audit/`: registro de ações sensíveis.

Esses diretórios são propostas, ainda não implementados. Extrações devem preservar transações e comportamento já testado.

## 7. Sequência de desenvolvimento

Trabalhar uma entrega verificável por vez, na ordem abaixo, ajustando prioridades quando o usuário orientar.

### Etapa 1 — Completar o acesso local

- [x] Troca da própria senha, exigindo a senha atual. Concluído e validado em 20/09/2026; evidências em `docs/progresso.md`.
- [x] Emissão/rotação de código de recuperação para administradores adicionais. Concluído e validado em 20/09/2026; evidências em `docs/progresso.md`.
- [x] Definir permissões por ação antes de oferecer perfis configuráveis. Concluído e validado em 20/09/2026; evidências em `docs/progresso.md`.
- [x] Registrar auditoria das alterações de usuários e permissões. Concluído e validado em 20/09/2026; evidências em `docs/progresso.md`.

**Etapa 1 concluída em 20/09/2026:** troca da própria senha, recuperação para administradores adicionais, matriz de permissões por ação e auditoria administrativa, com testes de serviço e interface.

**Concluída quando:** alterações exigirem autorização, sessões forem tratadas corretamente, o último administrador estiver protegido e os fluxos passarem em testes de serviço e interface.

### Etapa 2 — Completar cadastros e rastreabilidade

- [ ] Cadastro de fornecedores e vínculos necessários.
- [ ] Campos complementares de empresa, clientes e produtos.
- [ ] Auditoria de alterações e inativações relevantes.

**Concluída quando:** cadastro, busca, edição, inativação, persistência e migração estiverem validados sem perder dados existentes.

### Etapa 3 — Completar a operação de venda

- [ ] Descontos e acréscimos com regras e permissões definidas.
- [ ] Pagamentos divididos, ajustando consultas que hoje assumem um pagamento por venda.
- [ ] Cancelamento/devolução com justificativa e estorno de estoque/pagamento.
- [ ] Comprovante não fiscal para impressão/exportação.

**Concluída quando:** venda e estornos preservarem consistência entre pagamentos, estoque, caixa e histórico, inclusive em falhas simuladas.

### Etapa 4 — Financeiro e relatórios

- [ ] Contas a pagar, contas a receber e despesas.
- [ ] Baixas e relação explícita com movimentos de caixa, evitando duplicidade.
- [ ] Filtro de vendas por período e relatórios CSV/PDF.
- [ ] Indicadores financeiros baseados nos registros implementados.

**Concluída quando:** totais e saldos puderem ser conferidos a partir dos registros, com filtros e exportações testados.

### Etapa 5 — Recursos por segmento

- [ ] Roupas: tamanho, cor, variações e estoque por variação.
- [ ] Mercados: unidades, quantidades fracionadas e regras de arredondamento no PDV.
- [ ] Serviços: cadastro de serviços, ordens de serviço e consumo opcional de materiais.
- [ ] Definir uma configuração e um fluxo de demonstração para cada segmento atendido.

**Concluída por segmento quando:** suas operações específicas estiverem implementadas e verificadas. Integração com balança, emissão fiscal e outras integrações precisam de escopo próprio.

### Etapa 6 — Recuperação e distribuição

- [ ] Backup automático local e restauração de backups antigos com migração controlada.
- [ ] Logging e diagnóstico sem expor credenciais.
- [ ] Compilação e testes em Windows.
- [ ] Instalador com Qt, OpenSSL e demais dependências.
- [ ] Atualização preservando dados e procedimento de recuperação em falhas.
- [ ] Licenciamento com política offline definida.

**Concluída quando:** instalação em máquina limpa, atualização, backup e recuperação forem demonstrados em Windows.

### Etapa 7 — Preparação comercial

- [ ] Fixar escopo e limitações da primeira versão vendida.
- [ ] Manual, demonstração, versão e canal de suporte.
- [ ] Teste piloto usando dados de demonstração e cenários do público escolhido.
- [ ] Verificar condições atuais de anúncio e distribuição na plataforma antes da publicação.
- [ ] Preparar anúncio com funcionalidades efetivamente entregues.

**Concluída quando:** os critérios de [comercialização](docs/comercializacao.md) forem atendidos. Não há data de lançamento definida.

## 8. Próxima tarefa concreta

**Cadastro de fornecedores e vínculos necessários** (Etapa 2).

Escopo:

1. Cadastro de fornecedores com nome obrigatório e identificador único (documento/contato), busca, edição, inativação/reativação e paginação, seguindo o padrão dos cadastros existentes.
2. Definir e implementar os vínculos necessários nesta fase (por exemplo, produto ↔ fornecedor) com migração 8, avaliando impacto em backup: restauração direta passa a exigir esquema 8.
3. Permissão pela matriz existente (`catalog`), reutilizando o controle de acesso já testado.
4. Tela em **Cadastros** com o mesmo comportamento das demais seções.
5. Testes de serviço e interface, incluindo migração de banco anterior; atualizar documentação e ponto de continuidade.

Campos complementares de produtos/clientes e auditoria de cadastros ficam para os próximos itens da Etapa 2. Não alterar o banco real para executar testes.

## 9. Critério de conclusão de cada entrega

- [ ] Fluxo funcional acessível na interface quando aplicável.
- [ ] Validações e permissões aplicadas no C++.
- [ ] Migração e compatibilidade de backup revisadas quando houver mudança de dados.
- [ ] Compilação concluída e testes pertinentes aprovados.
- [ ] Interface conferida no tamanho mínimo suportado, quando alterada.
- [ ] Documentação, limitações e próxima tarefa atualizadas.
- [ ] Mudanças do Git revisadas; código versionado sem bancos, backups, segredos ou arquivos de compilação.

Comandos locais:

```bash
./scripts/dev.sh build
./scripts/dev.sh test
./scripts/dev.sh run
```

A última validação registrada aprovou os cinco conjuntos de testes em Linux. Isso não substitui a validação futura em Windows nem significa que o produto esteja pronto para comercialização.
