import eslint from '@eslint/js'
import nextPlugin from '@next/eslint-plugin-next'
import stylistic from '@stylistic/eslint-plugin'
import { defineConfig } from 'eslint/config'
import importPlugin from 'eslint-plugin-import'
import reactPlugin from 'eslint-plugin-react'
import reactHooksPlugin from 'eslint-plugin-react-hooks'
import tsEslint from 'typescript-eslint'

const eslintConfig = defineConfig([
  eslint.configs.recommended,
  tsEslint.configs.recommended,
  importPlugin.flatConfigs.recommended,
  importPlugin.flatConfigs.typescript,
  stylistic.configs.customize({
    indent: 2,
    quotes: 'single',
    jsx: true,
    semi: true,
    json: true,
  }),
  {
    ignores: [
      '*',
      '!src/**',
      'src/app/(payload)/admin/importMap.js',
      'src/app/(payload)/admin/*/layout.tsx',
      'src/app/(payload)/api/**/route.ts',
      'src/app/(payload)/layout.tsx',
    ],
  },
  {
    files: ['**/*.{js,jsx,ts,tsx,mjs,mts}'],
    languageOptions: {
      parser: tsEslint.parser,
      parserOptions: {
        project: './tsconfig.json',
      },
    },
    plugins: {
      '@stylistic': stylistic,
      react: reactPlugin,
      'react-hooks': reactHooksPlugin,
      '@next/next': nextPlugin,
    },
    settings: {
      react: {
        version: 'detect',
      },
      'import/resolver': {
        typescript: true,
        node: true,
      },
      'import/ignore': ['node_modules'],
    },
    rules: {
      ...nextPlugin.configs.recommended.rules,

      camelcase: [
        'error',
        {
          ignoreGlobals: true,
          properties: 'never',
          allow: ['^o_', 'node_*', 'npm_*', 'unstable_*', 'migration_*'],
        },
      ],
      curly: 'error',
      eqeqeq: 'error',

      'id-length': [
        'warn',
        {
          exceptions: ['a', 'b', 't', 'q', 'e', 'i', 'j', '_'],
          properties: 'never',
        },
      ],

      'import/named': 'error',
      'import/no-duplicates': 'error',

      'import/order': [
        'error',
        {
          groups: [
            'builtin',
            'external',
            'internal',
            'parent',
            'sibling',
            'index',
            'type',
          ],
          pathGroups: [
            {
              pattern: '@/**',
              group: 'internal',
              position: 'before',
            },
          ],
          pathGroupsExcludedImportTypes: ['builtin'],
          'newlines-between': 'never',
        },
      ],

      'no-unused-expressions': [
        'error',
        {
          allowShortCircuit: true,
          allowTernary: true,
        },
      ],

      'no-unused-vars': 'off',
      'prefer-template': 'error',

      '@next/next/no-img-element': 'off',
      '@next/next/no-html-link-for-pages': 'off',

      '@stylistic/array-element-newline': ['error', 'consistent'],
      '@stylistic/brace-style': [
        'error',
        '1tbs',
        { allowSingleLine: false },
      ],
      '@stylistic/comma-dangle': ['error', 'never'],
      '@stylistic/eol-last': 'error',
      '@stylistic/jsx-closing-bracket-location': 'warn',
      '@stylistic/jsx-quotes': ['error', 'prefer-double'],
      '@stylistic/object-curly-newline': [
        'error',
        { consistent: true, multiline: true },
      ],
      '@stylistic/object-property-newline': [
        'error',
        { allowAllPropertiesOnSameLine: true },
      ],
      '@stylistic/padding-line-between-statements': [
        'error',
        { blankLine: 'always', prev: '*', next: 'return' },
      ],
      '@stylistic/quote-props': ['error', 'as-needed'],
      '@stylistic/quotes': ['error', 'single'],
      '@stylistic/semi': ['error', 'always'],
      '@stylistic/space-infix-ops': [
        'error',
        {
          int32Hint: false,
        },
      ],
      '@stylistic/space-unary-ops': 'error',
      '@stylistic/template-curly-spacing': ['error', 'never'],

      '@typescript-eslint/ban-ts-comment': 'off',
      '@typescript-eslint/no-empty-object-type': 'warn',
      '@typescript-eslint/no-explicit-any': 'off',
      '@typescript-eslint/no-unused-expressions': [
        'error',
        {
          allowShortCircuit: true,
          allowTernary: true,
        },
      ],
      '@typescript-eslint/no-unused-vars': [
        'warn',
        {
          vars: 'all',
          args: 'after-used',
          ignoreRestSiblings: false,
          argsIgnorePattern: '^_',
          varsIgnorePattern: '^_',
          destructuredArrayIgnorePattern: '^_',
          caughtErrorsIgnorePattern: '^(_|ignore)',
        },
      ],
    },
  },
  {
    files: ['*.config.mjs'],
    languageOptions: {
      parserOptions: {
        project: null,
      },
    },
  },
])

export default eslintConfig