/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "creature_factory.h"
#include "monster.h"
#include "level.h"
#include "entity_set.h"
#include "effect.h"
#include "item_factory.h"
#include "creature_attributes.h"
#include "view_object.h"
#include "view_id.h"
#include "creature.h"
#include "game.h"
#include "name_generator.h"
#include "player_message.h"
#include "equipment.h"
#include "minion_activity_map.h"
#include "spell_map.h"
#include "tribe.h"
#include "monster_ai.h"
#include "sound.h"
#include "player.h"
#include "map_memory.h"
#include "body.h"
#include "attack_type.h"
#include "attack_level.h"
#include "attack.h"
#include "spell_map.h"
#include "item_type.h"
#include "item.h"
#include "furniture.h"
#include "experience_type.h"
#include "creature_debt.h"
#include "effect.h"
#include "game_event.h"

class BoulderController : public Monster {
  public:
  BoulderController(WCreature c, Vec2 dir) : Monster(c, MonsterAIFactory::idle()), direction(dir) {
    CHECK(direction.length4() == 1);
  }

  virtual void makeMove() override {
    Position nextPos = creature->getPosition().plus(direction);
    if (WCreature c = nextPos.getCreature()) {
      if (!c->getBody().isKilledByBoulder()) {
        if (nextPos.canEnterEmpty(creature)) {
          creature->swapPosition(direction);
          return;
        }
      } else {
        health -= c->getBody().getBoulderDamage();
        if (health <= 0) {
          nextPos.globalMessage(creature->getName().the() + " crashes on " + c->getName().the());
          nextPos.unseenMessage("You hear a crash");
          creature->dieNoReason();
          //c->takeDamage(Attack(creature, AttackLevel::MIDDLE, AttackType::HIT, 1000, AttrType::DAMAGE));
          return;
        } else {
          c->you(MsgType::KILLED_BY, creature->getName().the());
          c->dieWithAttacker(creature);
        }
      }
    }
    if (auto furniture = nextPos.getFurniture(FurnitureLayer::MIDDLE))
      if (furniture->canDestroy(creature->getMovementType(), DestroyAction::Type::BOULDER) &&
          *furniture->getStrength(DestroyAction::Type::BOULDER) <
          health * creature->getAttr(AttrType::DAMAGE)) {
        health -= *furniture->getStrength(DestroyAction::Type::BOULDER) /
            (double) creature->getAttr(AttrType::DAMAGE);
        creature->destroyImpl(direction, DestroyAction::Type::BOULDER);
      }
    if (auto action = creature->move(direction))
      action.perform(creature);
    else {
      nextPos.globalMessage(creature->getName().the() + " crashes on the " + nextPos.getName());
      nextPos.unseenMessage("You hear a crash");
      creature->dieNoReason();
      return;
    }
    health -= 0.2;
    if (health <= 0 && !creature->isDead())
      creature->dieNoReason();
  }

  virtual MessageGenerator& getMessageGenerator() const override {
    static MessageGenerator g(MessageGenerator::BOULDER);
    return g;
  }

  SERIALIZE_ALL(SUBCLASS(Monster), direction)
  SERIALIZATION_CONSTRUCTOR(BoulderController)

  private:
  Vec2 SERIAL(direction);
  double health = 1;
};

PCreature CreatureFactory::getRollingBoulder(TribeId tribe, Vec2 direction) {
  ViewObject viewObject(ViewId::BOULDER, ViewLayer::CREATURE);
  viewObject.setModifier(ViewObjectModifier::NO_UP_MOVEMENT);
  auto ret = makeOwner<Creature>(viewObject, tribe, CATTR(
            c.viewId = ViewId::BOULDER;
            c.attr[AttrType::DAMAGE] = 250;
            c.attr[AttrType::DEFENSE] = 250;
            c.body = Body::nonHumanoid(Body::Material::ROCK, Body::Size::HUGE);
            c.body->setDeathSound(none);
            c.permanentEffects[LastingEffect::BLIND] = 1;
            c.boulder = true;
            c.name = "boulder";
            ));
  ret->setController(makeOwner<BoulderController>(ret.get(), direction));
  return ret;
}

class SokobanController : public Monster {
  public:
  SokobanController(WCreature c) : Monster(c, MonsterAIFactory::idle()) {}

  virtual MessageGenerator& getMessageGenerator() const override {
    static MessageGenerator g(MessageGenerator::BOULDER);
    return g;
  }

  SERIALIZE_ALL(SUBCLASS(Monster));
  SERIALIZATION_CONSTRUCTOR(SokobanController);

  private:
};

PCreature CreatureFactory::getSokobanBoulder(TribeId tribe) {
  ViewObject viewObject(ViewId::BOULDER, ViewLayer::CREATURE);
  viewObject.setModifier(ViewObjectModifier::NO_UP_MOVEMENT).setModifier(ViewObjectModifier::REMEMBER);
  auto ret = makeOwner<Creature>(viewObject, tribe, CATTR(
            c.viewId = ViewId::BOULDER;
            c.attr[AttrType::DAMAGE] = 250;
            c.attr[AttrType::DEFENSE] = 250;
            c.body = Body::nonHumanoid(Body::Material::ROCK, Body::Size::HUGE);
            c.body->setDeathSound(none);
            c.body->setMinPushSize(Body::Size::LARGE);
            c.permanentEffects[LastingEffect::BLIND] = 1;
            c.boulder = true;
            c.name = "boulder";));
  ret->setController(makeOwner<SokobanController>(ret.get()));
  return ret;
}

CreatureAttributes CreatureFactory::getKrakenAttributes(ViewId id, const char* name) {
  return CATTR(
      c.viewId = id;
      c.body = Body::nonHumanoid(Body::Size::LARGE);
      c.body->setDeathSound(none);
      c.attr[AttrType::DAMAGE] = 28;
      c.attr[AttrType::DEFENSE] = 28;
      c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
      c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
      c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
      c.name = name;);
}

ViewId CreatureFactory::getViewId(CreatureId id) const {
  if (!idMap.count(id)) {
    auto c = fromId(id, TribeId::getMonster());
    idMap[id] = c->getViewObject().id();
  }
  return idMap.at(id);
}

CreatureFactory::CreatureFactory(NameGenerator* n) : nameGenerator(n) {
}

constexpr int maxKrakenLength = 15;

class KrakenController : public Monster {
  public:
  KrakenController(WCreature c) : Monster(c, MonsterAIFactory::monster()) {
  }

  KrakenController(WCreature c, WeakPointer<KrakenController> father, int length)
      : Monster(c, MonsterAIFactory::monster()), length(length), father(father) {
  }

  virtual bool dontReplaceInCollective() override {
    return true;
  }

  int getMaxSpawns() {
    if (father)
      return 1;
    else
      return 7;
  }

  virtual void onKilled(WConstCreature attacker) override {
    if (attacker) {
      if (father)
        attacker->secondPerson("You cut the kraken's tentacle");
      else
        attacker->secondPerson("You kill the kraken!");
    }
    for (WCreature c : spawns)
      if (!c->isDead())
        c->dieNoReason();
  }

  virtual MessageGenerator& getMessageGenerator() const override {
    static MessageGenerator kraken(MessageGenerator::KRAKEN);
    static MessageGenerator third(MessageGenerator::THIRD_PERSON);
    if (father)
      return kraken;
    else
      return third;
  }

  void pullEnemy(WCreature held) {
    if (Random.roll(3)) {
      held->you(MsgType::HAPPENS_TO, creature->getName().the() + " pulls");
      if (father) {
        held->setHeld(father->creature);
        Vec2 pullDir = held->getPosition().getDir(creature->getPosition());
        creature->dieNoReason(Creature::DropType::NOTHING);
        held->displace(pullDir);
      } else {
        held->you(MsgType::ARE, "eaten by " + creature->getName().the());
        held->dieNoReason();
      }
    }
  }

  WCreature getHeld() {
    for (auto pos : creature->getPosition().neighbors8())
      if (auto other = pos.getCreature())
        if (other->getHoldingCreature() == creature)
          return other;
    return nullptr;
  }

  WCreature getVisibleEnemy() {
    const int radius = 10;
    WCreature ret = nullptr;
    auto myPos = creature->getPosition();
    for (Position pos : creature->getPosition().getRectangle(Rectangle::centered(Vec2(0, 0), radius)))
      if (WCreature c = pos.getCreature())
        if (c->getAttributes().getCreatureId() != creature->getAttributes().getCreatureId() &&
            (!ret || ret->getPosition().dist8(myPos) > c->getPosition().dist8(myPos)) &&
            creature->canSee(c) && creature->isEnemy(c) && !c->getHoldingCreature())
          ret = c;
    return ret;
  }

  void considerAttacking(WCreature c) {
    auto pos = c->getPosition();
    Vec2 v = creature->getPosition().getDir(pos);
    if (v.length8() == 1) {
      c->you(MsgType::HAPPENS_TO, creature->getName().the() + " swings itself around");
      c->setHeld(creature);
    } else if (length < maxKrakenLength && Random.roll(2)) {
      pair<Vec2, Vec2> dirs = v.approxL1();
      vector<Vec2> moves;
      if (creature->getPosition().plus(dirs.first).canEnter(
            {{MovementTrait::WALK, MovementTrait::SWIM}}))
        moves.push_back(dirs.first);
      if (creature->getPosition().plus(dirs.second).canEnter(
            {{MovementTrait::WALK, MovementTrait::SWIM}}))
        moves.push_back(dirs.second);
      if (!moves.empty()) {
        Vec2 move = Random.choose(moves);
        ViewId viewId = creature->getPosition().plus(move).canEnter({MovementTrait::SWIM})
          ? ViewId::KRAKEN_WATER : ViewId::KRAKEN_LAND;
        auto spawn = makeOwner<Creature>(creature->getTribeId(),
              CreatureFactory::getKrakenAttributes(viewId, "kraken tentacle"));
        spawn->setController(makeOwner<KrakenController>(spawn.get(), getThis().dynamicCast<KrakenController>(),
            length + 1));
        spawns.push_back(spawn.get());
        creature->getPosition().plus(move).addCreature(std::move(spawn));
      }
    }
  }

  virtual void makeMove() override {
    for (WCreature c : spawns)
      if (c->isDead()) {
        spawns.removeElement(c);
        break;
      }
    if (spawns.empty()) {
      if (auto held = getHeld()) {
        pullEnemy(held);
      } else if (auto c = getVisibleEnemy()) {
        considerAttacking(c);
      } else if (father && Random.roll(5)) {
        creature->dieNoReason(Creature::DropType::NOTHING);
        return;
      }
    }
    creature->wait().perform(creature);
  }

  SERIALIZE_ALL(SUBCLASS(Monster), ready, spawns, father, length);
  SERIALIZATION_CONSTRUCTOR(KrakenController);

  private:
  int SERIAL(length) = 0;
  bool SERIAL(ready) = false;
  vector<WCreature> SERIAL(spawns);
  WeakPointer<KrakenController> SERIAL(father);
};

class ShopkeeperController : public Monster, public EventListener<ShopkeeperController> {
  public:
  ShopkeeperController(WCreature c, Rectangle area)
      : Monster(c, MonsterAIFactory::stayInLocation(area)), shopArea(area) {
  }

  vector<Position> getAllShopPositions() const {
    return shopArea.getAllSquares().transform([this](Vec2 v){ return Position(v, myLevel); });
  }

  bool isShopPosition(const Position& pos) {
    return pos.isSameLevel(myLevel) && pos.getCoord().inRectangle(shopArea);
  }

  virtual void makeMove() override {
    if (firstMove) {
      myLevel = creature->getLevel();
      subscribeTo(creature->getPosition().getModel());
      for (Position v : getAllShopPositions()) {
        for (WItem item : v.getItems())
          item->setShopkeeper(creature);
        v.clearItemIndex(ItemIndex::FOR_SALE);
      }
      firstMove = false;
    }
    if (!creature->getPosition().isSameLevel(myLevel)) {
      Monster::makeMove();
      return;
    }
    vector<Creature::Id> creatures;
    for (Position v : getAllShopPositions())
      if (WCreature c = v.getCreature()) {
        creatures.push_back(c->getUniqueId());
        if (!prevCreatures.contains(c) && !thieves.contains(c) && !creature->isEnemy(c)) {
          if (!debtors.contains(c))
            c->secondPerson("\"Welcome to " + creature->getName().firstOrBare() + "'s shop!\"");
          else {
            c->secondPerson("\"Pay your debt or... !\"");
            thiefCount.erase(c);
          }
        }
      }
    for (auto debtorId : copyOf(debtors))
      if (!creatures.contains(debtorId))
        for (auto pos : creature->getPosition().getRectangle(Rectangle::centered(Vec2(0, 0), 30)))
          if (auto debtor = pos.getCreature())
            if (debtor->getUniqueId() == debtorId) {
              debtor->privateMessage("\"Come back, you owe me " + toString(debtor->getDebt().getAmountOwed(creature)) +
                  " gold!\"");
              if (++thiefCount.getOrInit(debtor) == 4) {
                debtor->privateMessage("\"Thief! Thief!\"");
                creature->getTribe()->onItemsStolen(debtor);
                thiefCount.erase(debtor);
                debtors.erase(debtor);
                thieves.insert(debtor);
                for (WItem item : debtor->getEquipment().getItems())
                  item->setShopkeeper(nullptr);
                break;
              }
            }
    prevCreatures.clear();
    for (auto c : creatures)
      prevCreatures.insert(c);
    Monster::makeMove();
  }

  virtual void onItemsGiven(vector<WItem> items, WCreature from) override {
    int paid = items.filter(Item::classPredicate(ItemClass::GOLD)).size();
    from->getDebt().add(creature, -paid);
    if (from->getDebt().getAmountOwed(creature) <= 0)
      debtors.erase(from);
  }
  
  void onEvent(const GameEvent& event) {
    using namespace EventInfo;
    event.visit(
        [&](const ItemsAppeared& info) {
          if (isShopPosition(info.position)) {
            for (auto& it : info.items) {
              it->setShopkeeper(creature);
              info.position.clearItemIndex(ItemIndex::FOR_SALE);
            }
          }
        },
        [&](const ItemsPickedUp& info) {
          if (isShopPosition(info.creature->getPosition())) {
            for (auto& item : info.items)
              if (item->isShopkeeper(creature)) {
                info.creature->getDebt().add(creature, item->getPrice());
                debtors.insert(info.creature);
              }
          }
        },
        [&](const ItemsDropped& info) {
          if (isShopPosition(info.creature->getPosition())) {
            for (auto& item : info.items)
              if (item->isShopkeeper(creature)) {
                info.creature->getDebt().add(creature, -item->getPrice());
                if (info.creature->getDebt().getAmountOwed(creature) == 0)
                  debtors.erase(info.creature);
              }
          }
        },
        [&](const auto&) {}
    );
  }

  template <class Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & SUBCLASS(Monster) & SUBCLASS(EventListener);
    ar(prevCreatures, debtors, thiefCount, thieves, shopArea, myLevel, firstMove);
  }

  SERIALIZATION_CONSTRUCTOR(ShopkeeperController);

  private:
  EntitySet<Creature> SERIAL(prevCreatures);
  EntitySet<Creature> SERIAL(debtors);
  EntityMap<Creature, int> SERIAL(thiefCount);
  EntitySet<Creature> SERIAL(thieves);
  Rectangle SERIAL(shopArea);
  WLevel SERIAL(myLevel) = nullptr;
  bool SERIAL(firstMove) = true;
};

void CreatureFactory::addInventory(WCreature c, const vector<ItemType>& items) {
  for (ItemType item : items)
    c->take(item.get());
}

PCreature CreatureFactory::getShopkeeper(Rectangle shopArea, TribeId tribe) const {
  auto ret = makeOwner<Creature>(tribe,
      CATTR(
        c.viewId = ViewId::SHOPKEEPER;
        c.body = Body::humanoid(Body::Size::LARGE);
        c.attr[AttrType::DAMAGE] = 17;
        c.attr[AttrType::DEFENSE] = 20;
        c.chatReactionFriendly = "complains about high import tax"_s;
        c.chatReactionHostile = "\"Die!\""_s;
        c.name = "shopkeeper";
        c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));));
  ret->setController(makeOwner<ShopkeeperController>(ret.get(), shopArea));
  vector<ItemType> inventory(Random.get(20, 60), ItemType::GoldPiece{});
  inventory.push_back(ItemType::Sword{});
  inventory.push_back(ItemType::LeatherArmor{});
  inventory.push_back(ItemType::LeatherBoots{});
  inventory.push_back(ItemType::Potion{Effect::Heal{}});
  inventory.push_back(ItemType::Potion{Effect::Heal{}});
  addInventory(ret.get(), inventory);
  return ret;
}

class IllusionController : public DoNothingController {
  public:
  IllusionController(WCreature c, GlobalTime deathT) : DoNothingController(c), deathTime(deathT) {}

  virtual void onKilled(WConstCreature attacker) override {
    if (attacker)
      attacker->message("It was just an illusion!");
  }

  virtual void makeMove() override {
    if (*creature->getGlobalTime() >= deathTime) {
      creature->message("The illusion disappears.");
      creature->dieNoReason();
    } else
      creature->wait().perform(creature);
  }

  SERIALIZE_ALL(SUBCLASS(DoNothingController), deathTime)
  SERIALIZATION_CONSTRUCTOR(IllusionController)

  private:
  GlobalTime SERIAL(deathTime);
};

PCreature CreatureFactory::getIllusion(WCreature creature) {
  ViewObject viewObject(creature->getViewObject().id(), ViewLayer::CREATURE, "Illusion");
  viewObject.setModifier(ViewObject::Modifier::ILLUSION);
  auto ret = makeOwner<Creature>(viewObject, creature->getTribeId(), CATTR(
          c.viewId = ViewId::ROCK; //overriden anyway
          c.illusionViewObject = creature->getViewObject();
          c.illusionViewObject->setModifier(ViewObject::Modifier::INVISIBLE, false);
          c.body = Body::nonHumanoidSpirit(Body::Size::LARGE);
          c.body->setDeathSound(SoundId::MISSED_ATTACK);
          c.attr[AttrType::DAMAGE] = 20; // just so it's not ignored by creatures
          c.attr[AttrType::DEFENSE] = 1;
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.noAttackSound = true;
          c.canJoinCollective = false;
          c.name = creature->getName();));
  ret->setController(makeOwner<IllusionController>(ret.get(), *creature->getGlobalTime()
      + TimeInterval(Random.get(5, 10))));
  return ret;
}

REGISTER_TYPE(BoulderController)
REGISTER_TYPE(SokobanController)
REGISTER_TYPE(KrakenController)
REGISTER_TYPE(ShopkeeperController)
REGISTER_TYPE(IllusionController)
REGISTER_TYPE(ListenerTemplate<ShopkeeperController>)

PCreature CreatureFactory::get(CreatureAttributes attr, TribeId tribe, const ControllerFactory& factory) {
  auto ret = makeOwner<Creature>(tribe, std::move(attr));
  ret->setController(factory.get(ret.get()));
  return ret;
}

static ViewId getSpecialViewId(bool humanoid, bool large, bool body, bool wings) {
  static vector<ViewId> specialViewIds {
    ViewId::SPECIAL_BLBN,
    ViewId::SPECIAL_BLBW,
    ViewId::SPECIAL_BLGN,
    ViewId::SPECIAL_BLGW,
    ViewId::SPECIAL_BMBN,
    ViewId::SPECIAL_BMBW,
    ViewId::SPECIAL_BMGN,
    ViewId::SPECIAL_BMGW,
    ViewId::SPECIAL_HLBN,
    ViewId::SPECIAL_HLBW,
    ViewId::SPECIAL_HLGN,
    ViewId::SPECIAL_HLGW,
    ViewId::SPECIAL_HMBN,
    ViewId::SPECIAL_HMBW,
    ViewId::SPECIAL_HMGN,
    ViewId::SPECIAL_HMGW,
  };
  return specialViewIds[humanoid * 8 + (!large) * 4 + (!body) * 2 + wings];
}

static string getSpeciesName(bool humanoid, bool large, bool living, bool wings) {
  static vector<string> names {
    "devitablex",
    "owlbeast",
    "hellar dra",
    "marilisk",
    "gelaticorn",
    "mant eatur",
    "phanticore",
    "yeth horro",
    "yeth amon",
    "mantic dra",
    "unic cread",
    "under hulk",
    "nightshasa",
    "manananggal",
    "dire spawn",
    "shamander",
  };
  return names[humanoid * 8 + (!large) * 4 + (!living) * 2 + wings];
}

static optional<ItemType> getSpecialBeastAttack(bool large, bool living, bool wings) {
  static vector<optional<ItemType>> attacks {
    ItemType(ItemType::fangs(7)),
    ItemType(ItemType::fangs(7, Effect::Fire{})),
    ItemType(ItemType::fangs(7, Effect::Fire{})),
    ItemType(ItemType::fists(7)),
    ItemType(ItemType::fangs(7, Effect::Lasting{LastingEffect::POISON})),
    ItemType(ItemType::fangs(7)),
    ItemType(ItemType::fangs(7, Effect::Lasting{LastingEffect::POISON})),
    ItemType(ItemType::fists(7)),
  };
  return attacks[(!large) * 4 + (!living) * 2 + wings];
}

static EnumMap<BodyPart, int> getSpecialBeastBody(bool large, bool living, bool wings) {
  static vector<EnumMap<BodyPart, int>> parts {
    {
      { BodyPart::LEG, 2}},
    {
      { BodyPart::ARM, 2},
      { BodyPart::LEG, 2},
      { BodyPart::WING, 2},
      { BodyPart::HEAD, 1}},
    {
      { BodyPart::LEG, 4},
      { BodyPart::HEAD, 1}},
    {
      { BodyPart::ARM, 2},
      { BodyPart::WING, 2},
      { BodyPart::HEAD, 1}},
    {},
    { 
      { BodyPart::LEG, 2},
      { BodyPart::WING, 2},
      { BodyPart::HEAD, 1}},
    {
      { BodyPart::LEG, 8},
      { BodyPart::HEAD, 1}},
    { 
      { BodyPart::WING, 2},
      { BodyPart::HEAD, 1}},
  };
  return parts[(!large) * 4 + (!living) * 2 + wings];
}

static vector<LastingEffect> getResistanceAndVulnerability(RandomGen& random) {
  vector<LastingEffect> resistances {
      LastingEffect::MAGIC_RESISTANCE,
      LastingEffect::MELEE_RESISTANCE,
      LastingEffect::RANGED_RESISTANCE
  };
  vector<LastingEffect> vulnerabilities {
      LastingEffect::MAGIC_VULNERABILITY,
      LastingEffect::MELEE_VULNERABILITY,
      LastingEffect::RANGED_VULNERABILITY
  };
  vector<LastingEffect> ret;
  ret.push_back(Random.choose(resistances));
  vulnerabilities.removeIndex(*resistances.findElement(ret[0]));
  ret.push_back(Random.choose(vulnerabilities));
  return ret;
}

PCreature CreatureFactory::getSpecial(TribeId tribe, bool humanoid, bool large, bool living, bool wings,
    const ControllerFactory& factory) const {
  Body body = Body(humanoid, living ? Body::Material::FLESH : Body::Material::SPIRIT,
      large ? Body::Size::LARGE : Body::Size::MEDIUM);
  if (wings)
    body.addWings();
  string name = getSpeciesName(humanoid, large, living, wings);
  PCreature c = get(CATTR(
        c.viewId = getSpecialViewId(humanoid, large, living, wings);
        c.isSpecial = true;
        c.body = std::move(body);
        c.attr[AttrType::DAMAGE] = Random.get(18, 24);
        c.attr[AttrType::DEFENSE] = Random.get(18, 24);
        c.attr[AttrType::SPELL_DAMAGE] = Random.get(18, 24);
        for (auto effect : getResistanceAndVulnerability(Random))
          c.permanentEffects[effect] = 1;
        if (large) {
          c.attr[AttrType::DAMAGE] += 6;
          c.attr[AttrType::DEFENSE] += 2;
          c.attr[AttrType::SPELL_DAMAGE] -= 6;
        }
        if (humanoid) {
          c.skills.setValue(SkillId::WORKSHOP, Random.getDouble(0, 1));
          c.skills.setValue(SkillId::FORGE, Random.getDouble(0, 1));
          c.skills.setValue(SkillId::LABORATORY, Random.getDouble(0, 1));
          c.skills.setValue(SkillId::JEWELER, Random.getDouble(0, 1));
          c.skills.setValue(SkillId::FURNACE, Random.getDouble(0, 1));
          c.maxLevelIncrease[ExperienceType::MELEE] = 10;
          c.maxLevelIncrease[ExperienceType::SPELL] = 10;
        }
        if (humanoid) {
          c.chatReactionFriendly = "\"I am the mighty " + name + "\"";
          c.chatReactionHostile = "\"I am the mighty " + name + ". Die!\"";
        } else {
          c.chatReactionFriendly = c.chatReactionHostile = c.petReaction = "snarls."_s;
        }
        c.name = name;
        c.name->setStack(humanoid ? "legendary humanoid" : "legendary beast");
        c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DEMON));
        if (!humanoid) {
          c.body->setBodyParts(getSpecialBeastBody(large, living, wings));
          c.attr[AttrType::DAMAGE] += 5;
          c.attr[AttrType::DEFENSE] += 5;
          if (auto attack = getSpecialBeastAttack(large, living, wings))
            c.body->setIntrinsicAttack(BodyPart::HEAD, *attack);
        }
        if (Random.roll(3))
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
        ), tribe, factory);
  if (body.isHumanoid()) {
    if (Random.roll(4))
      c->take(ItemType(ItemType::Bow{}).get());
    c->take(Random.choose(
          ItemType(ItemType::Sword{}).setPrefixChance(1),
          ItemType(ItemType::BattleAxe{}).setPrefixChance(1),
          ItemType(ItemType::WarHammer{}).setPrefixChance(1))
        .get());
  }
  return c;
}

CreatureAttributes CreatureFactory::getAttributes(CreatureId id) const {
  auto ret = getAttributesFromId(id);
  ret.setCreatureId(id);
  return ret;
}

#define CREATE_LITERAL(NAME, SHORT) \
static pair<AttrType, int> operator "" _##SHORT(unsigned long long value) {\
  return {AttrType::NAME, value};\
}

CREATE_LITERAL(DAMAGE, dam)
CREATE_LITERAL(DEFENSE, def)
CREATE_LITERAL(SPELL_DAMAGE, spell_dam)
CREATE_LITERAL(RANGED_DAMAGE, ranged_dam)

#undef CREATE_LITERAL

CreatureAttributes CreatureFactory::getAttributesFromId(CreatureId id) const {
  if (id == "KEEPER_MAGE")
      return CATTR(
          c.viewId = ViewId::KEEPER1;
          c.attr = LIST(12_dam, 12_def, 20_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "wizard";
          c.viewIdUpgrades = LIST(ViewId::KEEPER2, ViewId::KEEPER3, ViewId::KEEPER4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::LABORATORY, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 7;
          c.maxLevelIncrease[ExperienceType::SPELL] = 12;
          c.spells->add(SpellId::HEAL_SELF);
      );
  else if (id == "KEEPER_MAGE_F")
      return CATTR(
          c.viewId = ViewId::KEEPER_F1;
          c.attr = LIST(12_dam, 12_def, 20_spell_dam );
          c.gender = Gender::female;
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "wizard";
          c.viewIdUpgrades = LIST(ViewId::KEEPER_F2, ViewId::KEEPER_F3, ViewId::KEEPER_F4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_FEMALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::LABORATORY, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 7;
          c.maxLevelIncrease[ExperienceType::SPELL] = 12;
          c.spells->add(SpellId::HEAL_SELF);
      );
  else if (id == "KEEPER_KNIGHT")
      return CATTR(
          c.viewId = ViewId::KEEPER_KNIGHT1;
          c.attr = LIST(20_dam, 16_def);
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "knight";
          c.viewIdUpgrades = LIST(ViewId::KEEPER_KNIGHT2, ViewId::KEEPER_KNIGHT3, ViewId::KEEPER_KNIGHT4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::FORGE, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.maxLevelIncrease[ExperienceType::SPELL] = 3;
          c.spells->add(SpellId::HEAL_SELF);
      );
  else if (id == "KEEPER_KNIGHT_F")
      return CATTR(
          c.viewId = ViewId::KEEPER_KNIGHT_F1;
          c.attr = LIST(20_dam, 16_def);
          c.gender = Gender::female;
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "knight";
          c.viewIdUpgrades = LIST(ViewId::KEEPER_KNIGHT_F2, ViewId::KEEPER_KNIGHT_F3, ViewId::KEEPER_KNIGHT_F4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_FEMALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::FORGE, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.maxLevelIncrease[ExperienceType::SPELL] = 3;
          c.spells->add(SpellId::HEAL_SELF);
      );
  else if (id == "KEEPER_KNIGHT_WHITE")
      return CATTR(
          c.viewId = ViewId::DUKE1;
          c.attr = LIST(20_dam, 16_def);
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "knight";
          c.viewIdUpgrades = LIST(ViewId::DUKE2, ViewId::DUKE3, ViewId::DUKE4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::FORGE, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.maxLevelIncrease[ExperienceType::SPELL] = 1;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "KEEPER_KNIGHT_WHITE_F")
      return CATTR(
          c.viewId = ViewId::DUKE_F1;
          c.attr = LIST(20_dam, 16_def);
          c.gender = Gender::female;
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "knight";
          c.viewIdUpgrades = LIST(ViewId::DUKE_F2, ViewId::DUKE_F3, ViewId::DUKE_F4);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_FEMALE));
          c.name->useFullTitle();
          c.skills.setValue(SkillId::FORGE, 0.2);
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.maxLevelIncrease[ExperienceType::SPELL] = 1;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "ADVENTURER")
      return CATTR(
          c.viewId = ViewId::PLAYER;
          c.attr = LIST(15_dam, 20_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "squire";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.name->useFullTitle();
          c.permanentEffects[LastingEffect::AMBUSH_SKILL] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 16;
          c.maxLevelIncrease[ExperienceType::SPELL] = 8;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 8;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "ADVENTURER_F")
      return CATTR(
          c.viewId = ViewId::PLAYER_F;
          c.gender = Gender::female;
          c.attr = LIST(15_dam, 20_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = "squire";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_FEMALE));
          c.name->useFullTitle();
          c.permanentEffects[LastingEffect::AMBUSH_SKILL] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 16;
          c.maxLevelIncrease[ExperienceType::SPELL] = 8;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 8;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "UNICORN")
      return CATTR(
          c.viewId = ViewId::UNICORN;
          c.attr = LIST(16_dam, 20_def, 20_spell_dam);
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(500);
          c.body->setHorseBodyParts(8);
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::UnicornHorn{}));
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::HEAL_OTHER);
          c.spells->add(SpellId::SUMMON_SPIRIT);
          c.chatReactionFriendly = "\"mhhhhhrrrr!\""_s;
          c.chatReactionHostile = "\"mhhhhhrrrr!\""_s;
          c.petReaction = "neighs"_s;
          c.name = "unicorn";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DEITY));
          c.name->setGroup("herd");
      );
  else if (id == "BANDIT")
      return CATTR(
          c.viewId = ViewId::BANDIT;
          c.attr = LIST(15_dam, 13_def);
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all law enforcement"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::MELEE] = 2;
 //       c.skills.insert(SkillId::DISARM_TRAPS);
          c.name = "bandit";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "GHOST")
      return CATTR(
          c.viewId = ViewId::GHOST;
          c.attr = LIST(35_def, 30_spell_dam );
          c.body = Body::nonHumanoidSpirit(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.chatReactionFriendly = "\"Wouuuouuu!!!\""_s;
          c.chatReactionHostile = "\"Wouuuouuu!!!\""_s;
          c.name = "ghost";
      );
  else if (id == "SPIRIT")
      return CATTR(
          c.viewId = ViewId::SPIRIT;
          c.attr = LIST(35_def, 30_spell_dam );
          c.body = Body::nonHumanoidSpirit(Body::Size::LARGE);
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(ItemType::spellHit(10)));
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.chatReactionFriendly = "\"Wouuuouuu!!!\""_s;
          c.chatReactionHostile = "\"Wouuuouuu!!!\""_s;
          c.name = "ancient spirit";
      );
  else if (id == "LOST_SOUL")
      return CATTR(
          c.body = Body::nonHumanoidSpirit(Body::Size::LARGE);
          c.body->setDeathSound(none);
          c.viewId = ViewId::GHOST;
          c.attr = LIST(25_def, 5_spell_dam );
          c.courage = 100;
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(
              ItemType::touch(Effect(Effect::Lasting{LastingEffect::INSANITY}), Effect(Effect::Suicide{}))));
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.chatReactionFriendly = "\"Wouuuouuu!!!\""_s;
          c.chatReactionHostile = "\"Wouuuouuu!!!\""_s;
          c.name = "ghost";
      );
  else if (id == "SUCCUBUS")
      return CATTR(
          c.attr = LIST(25_def, 5_spell_dam );
          c.viewId = ViewId::SUCCUBUS;
          c.body = Body::humanoidSpirit(Body::Size::LARGE);
          c.body->addWings();
          c.permanentEffects[LastingEffect::COPULATION_SKILL] = 1;
          c.body->getIntrinsicAttacks().clear();
          c.body->setIntrinsicAttack(BodyPart::ARM, IntrinsicAttack(
              ItemType::touch(Effect::Lasting{LastingEffect::PEACEFULNESS})));
          c.gender = Gender::female;
          c.courage = -1;
          c.name = CreatureName("succubus", "succubi");
      );
  else if (id == "DOPPLEGANGER")
      return CATTR(
          c.viewId = ViewId::DOPPLEGANGER;
          c.attr = LIST(25_def, 5_spell_dam );
          c.body = Body::nonHumanoidSpirit(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::CONSUMPTION_SKILL] = 1;
          c.name = "doppelganger";
      );
  else if (id == "WITCH")
      return CATTR(
          c.viewId = ViewId::WITCH;
          c.attr = LIST(14_dam, 14_def, 20_spell_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.name = CreatureName("witch", "witches");
          c.name->setFirst("Cornelia");
          c.gender = Gender::female;
          c.chatReactionFriendly = "curses all humans"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.skills.setValue(SkillId::LABORATORY, 0.7);
          c.maxLevelIncrease[ExperienceType::SPELL] = 4;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "WITCHMAN")
      return CATTR(
          c.viewId = ViewId::WITCHMAN;
          c.attr = LIST(30_dam, 30_def, 20_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.name = CreatureName("witchman", "witchmen");
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.gender = Gender::male;
          c.chatReactionFriendly = "curses all monsters"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "CYCLOPS")
      return CATTR(
          c.viewId = ViewId::CYCLOPS;
          c.attr = LIST(34_dam, 40_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.name = CreatureName("cyclops", "cyclopes");
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::CYCLOPS));
          c.maxLevelIncrease[ExperienceType::MELEE] = 5;
      );
  else if (id == "DEMON_DWELLER")
      return CATTR(
          c.viewId = ViewId::DEMON_DWELLER;
          c.attr = LIST(25_dam, 30_def, 35_spell_dam );
          c.body = Body::humanoidSpirit(Body::Size::LARGE);
          c.body->addWings();
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.courage = 100;
          c.gender = Gender::male;
          c.spells->add(SpellId::BLAST);
          c.chatReactionFriendly = "\"Kneel before us!\""_s;
          c.chatReactionHostile = "\"Face your death!\""_s;
          c.name = "demon dweller";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DEMON));
          c.name->setGroup("pack");
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.maxLevelIncrease[ExperienceType::SPELL] = 4;
      );
  else if (id == "DEMON_LORD")
      return CATTR(
          c.viewId = ViewId::DEMON_LORD;
          c.attr = LIST(40_dam, 45_def, 50_spell_dam );
          c.body = Body::humanoidSpirit(Body::Size::LARGE);
          c.body->addWings();
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.courage = 100;
          c.gender = Gender::male;
          c.spells->add(SpellId::BLAST);
          c.chatReactionFriendly = "\"Kneel before us!\""_s;
          c.chatReactionHostile = "\"Face your death!\""_s;
          c.name = "Demon Lord";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DEMON));
          c.name->setGroup("pack");
          c.maxLevelIncrease[ExperienceType::SPELL] = 7;
      );
  else if (id == "MINOTAUR")
      return CATTR(
          c.viewId = ViewId::MINOTAUR;
          c.attr = LIST(35_dam, 45_def );
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.maxLevelIncrease[ExperienceType::MELEE] = 5;
          c.name = "minotaur";
      );
  else if (id == "SOFT_MONSTER")
      return CATTR(
          c.viewId = ViewId::SOFT_MONSTER;
          c.attr = LIST(45_dam, 25_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.courage = -1;
          c.name = "soft monster";
      );
  else if (id == "HYDRA")
      return CATTR(
          c.viewId = ViewId::HYDRA;
          c.attr = LIST(27_dam, 45_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(8, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.name = "hydra";
      );
  else if (id == "SHELOB")
      return CATTR(
          c.viewId = ViewId::SHELOB;
          c.attr = LIST(40_dam, 38_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.body->setBodyParts({{BodyPart::LEG, 8}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(
              ItemType::fangs(8, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::SPIDER_SKILL] = 1;
          c.name = "giant spider";
      );
  else if (id == "GREEN_DRAGON")
      return CATTR(
          c.viewId = ViewId::GREEN_DRAGON;
          c.attr = LIST(40_dam, 40_def );
          c.body = Body::nonHumanoid(Body::Size::HUGE);
          c.body->setHorseBodyParts(7);
          c.body->addWings();
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(12, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::RANGED_VULNERABILITY] = 1;
          c.name = "green dragon";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DRAGON));
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::CURE_POISON);
          c.spells->add(SpellId::DECEPTION);
          c.spells->add(SpellId::SPEED_SELF);
          c.name->setStack("dragon");
      );
  else if (id == "RED_DRAGON")
      return CATTR(
          c.viewId = ViewId::RED_DRAGON;
          c.attr = LIST(40_dam, 42_def );
          c.body = Body::nonHumanoid(Body::Size::HUGE);
          c.body->setHorseBodyParts(8);
          c.body->addWings();
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(15, Effect::Fire{})));
          c.name = "red dragon";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DRAGON));
          c.permanentEffects[LastingEffect::RANGED_VULNERABILITY] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::CURE_POISON);
          c.spells->add(SpellId::DECEPTION);
          c.spells->add(SpellId::SPEED_SELF);
          c.spells->add(SpellId::FIREBALL_DRAGON);
          c.name->setStack("dragon");
      );
  else if (id == "KNIGHT_PLAYER")
      return CATTR(
          c.viewId = ViewId::KNIGHT;
          c.attr = LIST(16_dam, 14_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.maxLevelIncrease[ExperienceType::MELEE] = 7;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.skills.setValue(SkillId::WORKSHOP, 0.3);
          c.skills.setValue(SkillId::FORGE, 0.3);
          c.name = "knight";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "JESTER_PLAYER")
      return CATTR(
          c.viewId = ViewId::JESTER;
          c.attr = LIST(8_dam, 8_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "jester";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "ARCHER_PLAYER")
      return CATTR(
          c.viewId = ViewId::ARCHER;
          c.attr = LIST(10_dam, 10_def, 10_ranged_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 7;
          c.name = "archer";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "PRIEST_PLAYER")
      return CATTR(
          c.viewId = ViewId::PRIEST;
          c.attr = LIST(12_dam, 8_def, 16_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::SPELL] = 9;
          c.name = "priest";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "GNOME_PLAYER")
      return CATTR(
          c.viewId = ViewId::GNOME;
          c.attr = LIST(10_dam, 10_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "talks about crafting"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.skills.setValue(SkillId::LABORATORY, 0.3);
          c.skills.setValue(SkillId::WORKSHOP, 0.6);
          c.skills.setValue(SkillId::FORGE, 0.6);
          c.skills.setValue(SkillId::JEWELER, 0.6);
          c.name = "gnome";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DWARF));
      );
  else if (id == "PESEANT_PLAYER")
      return CATTR(
          if (Random.roll(2)) {
            c.viewId = ViewId::PESEANT_WOMAN;
            c.gender = Gender::female;
          } else
            c.viewId = ViewId::PESEANT;
          c.attr = LIST(14_dam, 12_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Heeelp!\""_s;
          c.skills.setValue(SkillId::DIGGING, 0.1);
          c.name = "peasant";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "KNIGHT")
      return CATTR(
          c.viewId = ViewId::KNIGHT;
          c.attr = LIST(36_dam, 28_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "knight";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "JESTER")
      return CATTR(
          c.viewId = ViewId::JESTER;
          c.attr = LIST(8_dam, 8_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "jester";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "DUKE")
      return CATTR(
          c.viewId = ViewId::DUKE4;
          c.attr = LIST(43_dam, 32_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::MELEE] = 3;
          c.courage = 1;
          c.name = "Duke of " + nameGenerator->getNext(NameGeneratorId::WORLD);
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_MALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "ARCHER")
      return CATTR(
          c.viewId = ViewId::ARCHER;
          c.attr = LIST(17_dam, 22_def, 30_ranged_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 4;
          c.name = "archer";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "PRIEST")
      return CATTR(
          c.viewId = ViewId::PRIEST;
          c.attr = LIST(15_dam, 15_def, 34_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::SPEED_SELF);
          c.spells->add(SpellId::DEF_BONUS);
          c.spells->add(SpellId::BLAST);
          c.spells->add(SpellId::MAGIC_MISSILE);
          c.spells->add(SpellId::HEAL_OTHER);
          c.maxLevelIncrease[ExperienceType::SPELL] = 2;
          c.name = "priest";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "WARRIOR")
      return CATTR(
          c.viewId = ViewId::WARRIOR;
          c.attr = LIST(27_dam, 19_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.maxLevelIncrease[ExperienceType::MELEE] = 5;
          c.skills.setValue(SkillId::WORKSHOP, 0.3);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "warrior";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "SHAMAN")
      return CATTR(
          c.viewId = ViewId::SHAMAN;
          c.attr = LIST(27_dam, 19_def, 30_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.courage = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::SPEED_SELF);
          c.spells->add(SpellId::DEF_BONUS);
          c.spells->add(SpellId::SUMMON_SPIRIT);
          c.spells->add(SpellId::BLAST);
          c.spells->add(SpellId::HEAL_OTHER);
          c.maxLevelIncrease[ExperienceType::SPELL] = 5;
          c.name = "shaman";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "PESEANT")
      return CATTR(
          if (Random.roll(2)) {
            c.viewId = ViewId::PESEANT_WOMAN;
            c.gender = Gender::female;
          } else
            c.viewId = ViewId::PESEANT;
          c.attr = LIST(14_dam, 12_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Heeelp!\""_s;
          c.permanentEffects[LastingEffect::CROPS_SKILL] = 1;
          c.skills.setValue(SkillId::DIGGING, 0.1);
          c.name = "peasant";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "CHILD")
      return CATTR(
          c.viewId = ViewId::CHILD;
          c.attr = LIST(8_dam, 8_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "\"plaaaaay!\""_s;
          c.chatReactionHostile = "\"Heeelp!\""_s;
          c.permanentEffects[LastingEffect::CROPS_SKILL] = 1;
          c.name = CreatureName("child", "children");
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "SPIDER_FOOD")
      return CATTR(
          c.viewId = ViewId::CHILD;
          c.attr = LIST(2_dam, 2_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.permanentEffects[LastingEffect::ENTANGLED] = 1;
          c.permanentEffects[LastingEffect::BLIND] = 1;
          c.chatReactionFriendly = "\"Put me out of my misery PLEASE!\""_s;
          c.chatReactionHostile = "\"End my torture!\""_s;
          c.deathDescription = "dead, released from unthinkable agony"_s;
          c.name = CreatureName("child", "children");
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "PESEANT_PRISONER")
      return CATTR(
          if (Random.roll(2)) {
            c.viewId = ViewId::PESEANT_WOMAN;
            c.gender = Gender::female;
          } else
            c.viewId = ViewId::PESEANT;
          c.attr = LIST(14_dam, 12_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Heeelp!\""_s;
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.skills.setValue(SkillId::DIGGING, 0.3);
          c.name = "peasant";
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "HALLOWEEN_KID")
      return CATTR(
          c.viewId = Random.choose(ViewId::HALLOWEEN_KID1,
              ViewId::HALLOWEEN_KID2, ViewId::HALLOWEEN_KID3,ViewId::HALLOWEEN_KID4);
          c.attr = LIST(8_dam, 8_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "\"Trick or treat!\""_s;
          c.chatReactionHostile = "\"Trick or treat!\""_s;
          c.name = CreatureName("child", "children");
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "CLAY_GOLEM")
      return CATTR(
          c.viewId = ViewId::CLAY_GOLEM;
          c.attr = LIST(17_dam, 19_def );
          c.body = Body::nonHumanoid(Body::Material::CLAY, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(2);
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.name = "clay golem";
      );
  else if (id == "STONE_GOLEM")
      return CATTR(
          c.viewId = ViewId::STONE_GOLEM;
          c.attr = LIST(19_dam, 23_def );
          c.body = Body::nonHumanoid(Body::Material::ROCK, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(4);
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.name = "stone golem";
      );
  else if (id == "IRON_GOLEM")
      return CATTR(
          c.viewId = ViewId::IRON_GOLEM;
          c.attr = LIST(23_dam, 30_def );
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.body = Body::nonHumanoid(Body::Material::IRON, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(7);
          c.name = "iron golem";
      );
  else if (id == "LAVA_GOLEM")
      return CATTR(
          c.viewId = ViewId::LAVA_GOLEM;
          c.attr = LIST(26_dam, 36_def );
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.body = Body::nonHumanoid(Body::Material::LAVA, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(8);
          c.body->setIntrinsicAttack(BodyPart::ARM, IntrinsicAttack(ItemType::fists(10, Effect::Fire{})));
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.name = "lava golem";
      );
  else if (id == "ADA_GOLEM")
      return CATTR(
          c.viewId = ViewId::ADA_GOLEM;
          c.attr = LIST(36_dam, 36_def );
          c.permanentEffects[LastingEffect::NAVIGATION_DIGGING_SKILL] = 1;
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.body = Body::nonHumanoid(Body::Material::ADA, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(8);
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.name = "adamantine golem";
      );
  else if (id == "AUTOMATON")
      return CATTR(
          c.viewId = ViewId::AUTOMATON;
          c.attr = LIST(40_dam, 40_def );
          c.permanentEffects[LastingEffect::MELEE_RESISTANCE] = 1;
          c.body = Body::nonHumanoid(Body::Material::IRON, Body::Size::LARGE);
          c.body->setHumanoidBodyParts(10);
          c.name = "automaton";
      );
  else if (id == "ZOMBIE")
      return CATTR(
          c.viewId = ViewId::ZOMBIE;
          c.attr = LIST(14_dam, 17_def );
          c.body = Body::humanoid(Body::Material::UNDEAD_FLESH, Body::Size::LARGE);
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 3;
          c.name = "zombie";
          c.hatedByEffect = LastingEffect::HATE_UNDEAD;
      );
  else if (id == "SKELETON")
      return CATTR(
          c.viewId = ViewId::SKELETON;
          c.attr = LIST(17_dam, 13_def, 5_ranged_dam);
          c.body = Body::humanoid(Body::Material::BONE, Body::Size::LARGE);
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 3;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 4;
          c.name = "skeleton";
          c.hatedByEffect = LastingEffect::HATE_UNDEAD;
      );
  else if (id == "VAMPIRE")
      return CATTR(
          c.viewId = ViewId::VAMPIRE;
          c.attr = LIST(17_dam, 17_def, 17_spell_dam );
          c.body = Body::humanoid(Body::Material::UNDEAD_FLESH, Body::Size::LARGE);
          c.chatReactionFriendly = "\"All men be cursed!\""_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 7;
          c.maxLevelIncrease[ExperienceType::SPELL] = 7;
          c.name = "vampire";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::VAMPIRE));
          c.hatedByEffect = LastingEffect::HATE_UNDEAD;
      );
  else if (id == "VAMPIRE_LORD")
      return CATTR(
          c.viewId = ViewId::VAMPIRE_LORD;
          c.attr = LIST(17_dam, 23_def, 27_spell_dam );
          c.body = Body::humanoid(Body::Material::UNDEAD_FLESH, Body::Size::LARGE);
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.name = "vampire lord";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::VAMPIRE));
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.maxLevelIncrease[ExperienceType::SPELL] = 12;
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::DARKNESS_SOURCE] = 1;
          for (SpellId id : Random.chooseN(Random.get(3, 6), {SpellId::CIRCULAR_BLAST, SpellId::DEF_BONUS,
              SpellId::DAM_BONUS, SpellId::DECEPTION, SpellId::DECEPTION, SpellId::TELEPORT}))
            c.spells->add(id);
          c.chatReactionFriendly = c.chatReactionHostile =
              "\"There are times when you simply cannot refuse a drink!\""_s;
          c.hatedByEffect = LastingEffect::HATE_UNDEAD;
      );
  else if (id == "MUMMY")
      return CATTR(
          c.viewId = ViewId::MUMMY;
          c.attr = LIST(15_dam, 14_def, 10_spell_dam );
          c.body = Body::humanoid(Body::Material::UNDEAD_FLESH, Body::Size::LARGE);
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.permanentEffects[LastingEffect::SLOW_TRAINING] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 25;
          c.name = CreatureName("mummy", "mummies");
          c.hatedByEffect = LastingEffect::HATE_UNDEAD;
      );
  else if (id == "ORC")
      return CATTR(
          c.viewId = ViewId::ORC;
          c.attr = LIST(16_dam, 14_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.chatReactionFriendly = "curses all elves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.skills.setValue(SkillId::WORKSHOP, 0.3);
          c.skills.setValue(SkillId::FORGE, 0.3);
          c.maxLevelIncrease[ExperienceType::MELEE] = 7;
          c.name = "orc";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::ORC));
          c.hatedByEffect = LastingEffect::HATE_GREENSKINS;
      );
  else if (id == "ORC_SHAMAN")
      return CATTR(
          c.viewId = ViewId::ORC_SHAMAN;
          c.attr = LIST(12_dam, 8_def, 16_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.skills.setValue(SkillId::LABORATORY, 0.7);
          c.chatReactionFriendly = "curses all elves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.maxLevelIncrease[ExperienceType::SPELL] = 7;
          c.name = "orc shaman";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::ORC));
          c.hatedByEffect = LastingEffect::HATE_GREENSKINS;
      );
  else if (id == "HARPY")
      return CATTR(
          c.viewId = ViewId::HARPY;
          c.attr = LIST(13_dam, 16_def, 15_ranged_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->addWings();
          c.skills.setValue(SkillId::LABORATORY, 0.3);
          c.gender = Gender::female;
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 7;
          c.name = CreatureName("harpy", "harpies");
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::ORC));
          c.hatedByEffect = LastingEffect::HATE_GREENSKINS;
      );
  else if (id == "KOBOLD")
      return CATTR(
          c.viewId = ViewId::KOBOLD;
          c.attr = LIST(14_dam, 16_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.chatReactionFriendly = "talks about digging"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "kobold";
      );
  else if (id == "GNOME")
      return CATTR(
          c.viewId = ViewId::GNOME;
          c.attr = LIST(12_dam, 13_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.skills.setValue(SkillId::JEWELER, 0.5);
          c.chatReactionFriendly = "talks about digging"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "gnome";);
  else if (id == "GNOME_CHIEF")
      return CATTR(
          c.viewId = ViewId::GNOME_BOSS;
          c.attr = LIST(15_dam, 16_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.skills.setValue(SkillId::JEWELER, 1);
          c.chatReactionFriendly = "talks about digging"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "gnome chief";);
  else if (id == "GOBLIN")
      return CATTR(
          c.viewId = ViewId::GOBLIN;
          c.attr = LIST(12_dam, 13_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "talks about crafting"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::DISARM_TRAPS_SKILL] = 1;
          c.skills.setValue(SkillId::LABORATORY, 0.3);
          c.skills.setValue(SkillId::WORKSHOP, 0.6);
          c.skills.setValue(SkillId::FORGE, 0.6);
          c.skills.setValue(SkillId::JEWELER, 0.6);
          c.skills.setValue(SkillId::FURNACE, 0.6);
          c.name = "goblin";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::ORC));
          c.hatedByEffect = LastingEffect::HATE_GREENSKINS;
      );
  else if (id == "IMP")
      return CATTR(
          c.viewId = ViewId::IMP;
          c.attr = LIST(5_dam, 15_def );
          c.body = Body::humanoidSpirit(Body::Size::SMALL);
          c.courage = -1;
          c.noChase = true;
          c.cantEquip = true;
          c.skills.setValue(SkillId::DIGGING, 0.4);
          c.chatReactionFriendly = "talks about digging"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.permanentEffects[LastingEffect::NO_CARRY_LIMIT] = 1;
          c.name = "imp";
      );
  else if (id == "OGRE")
      return CATTR(
          c.viewId = ViewId::OGRE;
          c.attr = LIST(18_dam, 18_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->setWeight(140);
          c.name = "ogre";
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::ORC));
          c.skills.setValue(SkillId::WORKSHOP, 0.5);
          c.skills.setValue(SkillId::FORGE, 0.5);
          c.skills.setValue(SkillId::FURNACE, 0.9);
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.hatedByEffect = LastingEffect::HATE_GREENSKINS;
      );
  else if (id == "CHICKEN")
      return CATTR(
          c.viewId = ViewId::CHICKEN;
          c.attr = LIST(2_dam, 2_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(3);
          c.body->setMinionFood();
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.name = "chicken";
      );
  else if (id == "DWARF")
      return CATTR(
          c.viewId = ViewId::DWARF;
          c.attr = LIST(21_dam, 25_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.body->setWeight(90);
          c.name = CreatureName("dwarf", "dwarves");
          c.permanentEffects[LastingEffect::NAVIGATION_DIGGING_SKILL] = 1;
          c.skills.setValue(SkillId::FORGE, 0.8);
          c.skills.setValue(SkillId::FURNACE, 0.8);
          c.maxLevelIncrease[ExperienceType::MELEE] = 2;
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DWARF));
          c.chatReactionFriendly = "curses all orcs"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.hatedByEffect = LastingEffect::HATE_DWARVES;
      );
  else if (id == "DWARF_FEMALE")
      return CATTR(
          c.viewId = ViewId::DWARF_FEMALE;
          c.attr = LIST(21_dam, 25_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.body->setWeight(90);
          c.name = CreatureName("dwarf", "dwarves");
          c.permanentEffects[LastingEffect::NAVIGATION_DIGGING_SKILL] = 1;
          c.skills.setValue(SkillId::WORKSHOP, 0.5);
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DWARF));
          c.chatReactionFriendly = "curses all orcs"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.gender = Gender::female;
          c.hatedByEffect = LastingEffect::HATE_DWARVES;
      );
  else if (id == "DWARF_BARON")
      return CATTR(
          c.viewId = ViewId::DWARF_BARON;
          c.attr = LIST(28_dam, 32_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.body->setWeight(120);
          c.chatReactionFriendly = "curses all orcs"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::NAVIGATION_DIGGING_SKILL] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 10;
          c.permanentEffects[LastingEffect::MAGIC_VULNERABILITY] = 1;
          c.courage = 1;
          c.name = "dwarf baron";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DWARF));
          c.hatedByEffect = LastingEffect::HATE_DWARVES;
      );
  else if (id == "LIZARDMAN")
      return CATTR(
          c.viewId = ViewId::LIZARDMAN;
          c.attr = LIST(20_dam, 14_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(5, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 5;
          c.chatReactionFriendly = "curses all humans"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = CreatureName("lizardman", "lizardmen");
      );
  else if (id == "LIZARDLORD")
      return CATTR(
          c.viewId = ViewId::LIZARDLORD;
          c.attr = LIST(30_dam, 16_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.permanentEffects[LastingEffect::POISON_RESISTANT] = 1;
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(8, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 10;
          c.chatReactionFriendly = "curses all humans"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.courage = 1;
          c.name = "lizardman chief";
      );
  else if (id == "ELF")
      return CATTR(
          c.viewId = Random.choose(ViewId::ELF, ViewId::ELF_WOMAN);
          c.attr = LIST(14_dam, 6_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.spells->add(SpellId::HEAL_SELF);
          c.skills.setValue(SkillId::JEWELER, 0.9);
          c.maxLevelIncrease[ExperienceType::SPELL] = 1;
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.name = CreatureName("elf", "elves");
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "ELF_ARCHER")
      return CATTR(
          c.viewId = ViewId::ELF_ARCHER;
          c.attr = LIST(18_dam, 12_def, 25_ranged_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 3;
          c.name = "elven archer";
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "ELF_CHILD")
      return CATTR(
          c.viewId = ViewId::ELF_CHILD;
          c.attr = LIST(6_dam, 6_def );
          c.body = Body::humanoid(Body::Size::SMALL);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.name = CreatureName("elf child", "elf children");
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "ELF_LORD")
      return CATTR(
          c.viewId = ViewId::ELF_LORD;
          c.attr = LIST(22_dam, 14_def, 16_spell_dam, 30_ranged_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::HEAL_OTHER);
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::SPEED_SELF);
          c.spells->add(SpellId::DAM_BONUS);
          c.spells->add(SpellId::DEF_BONUS);
          c.spells->add(SpellId::BLAST);
          c.maxLevelIncrease[ExperienceType::SPELL] = 4;
          c.maxLevelIncrease[ExperienceType::MELEE] = 4;
          c.name = "elf lord";
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "DARK_ELF")
      return CATTR(
          c.viewId = Random.choose(ViewId::DARK_ELF, ViewId::DARK_ELF_WOMAN);
          c.attr = LIST(14_dam, 6_def );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.name = CreatureName("dark elf", "dark elves");
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "DARK_ELF_WARRIOR")
      return CATTR(
          c.viewId = ViewId::DARK_ELF_WARRIOR;
          c.attr = LIST(18_dam, 12_def, 6_spell_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 5;
          c.maxLevelIncrease[ExperienceType::SPELL] = 5;
          c.name = CreatureName("dark elf", "dark elves");
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "DARK_ELF_CHILD")
      return CATTR(
          c.viewId = ViewId::DARK_ELF_CHILD;
          c.attr = LIST(6_dam, 6_def );
          c.body = Body::humanoid(Body::Size::SMALL);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.name = CreatureName("dark elf child", "dark elf children");
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "DARK_ELF_LORD")
      return CATTR(
          c.viewId = ViewId::DARK_ELF_LORD;
          c.attr = LIST(22_dam, 14_def, 16_spell_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.chatReactionFriendly = "curses all dwarves"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::HEAL_OTHER);
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.spells->add(SpellId::HEAL_SELF);
          c.spells->add(SpellId::SPEED_SELF);
          c.spells->add(SpellId::DEF_BONUS);
          c.spells->add(SpellId::DAM_BONUS);
          c.spells->add(SpellId::BLAST);
          c.name = "dark elf lord";
          c.hatedByEffect = LastingEffect::HATE_ELVES;
      );
  else if (id == "DRIAD")
      return CATTR(
          c.viewId = ViewId::DRIAD;
          c.attr = LIST(6_dam, 14_def, 25_ranged_dam );
          c.body = Body::humanoid(Body::Size::MEDIUM);
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all humans"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.spells->add(SpellId::HEAL_SELF);
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.maxLevelIncrease[ExperienceType::ARCHERY] = 4;
          c.name = "driad";
      );
  else if (id == "HORSE")
      return CATTR(
          c.viewId = ViewId::HORSE;
          c.attr = LIST(16_dam, 7_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(500);
          c.body->setHorseBodyParts(2);
          c.noChase = true;
          c.petReaction = "neighs"_s;
          c.name = "horse";
      );
  else if (id == "COW")
      return CATTR(
          c.viewId = ViewId::COW;
          c.attr = LIST(10_dam, 7_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.body->setHorseBodyParts(2);
          c.noChase = true;
          c.petReaction = "\"Mooooooooooooooooooooooooooo!\""_s;
          c.name = "cow";
      );
  else if (id == "DONKEY")
      return CATTR(
          c.viewId = ViewId::DONKEY;
          c.attr = LIST(10_dam, 7_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(200);
          c.body->setHorseBodyParts(2);
          c.body->setDeathSound(SoundId::DYING_DONKEY);
          c.noChase = true;
          c.name = "donkey";
      );
  else if (id == "PIG")
      return CATTR(
          c.viewId = ViewId::PIG;
          c.attr = LIST(5_dam, 2_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(150);
          c.body->setHorseBodyParts(2);
          c.body->setMinionFood();
          c.body->setDeathSound(SoundId::DYING_PIG);
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.noChase = true;
          c.name = "pig";
      );
  else if (id == "GOAT")
      return CATTR(
          c.viewId = ViewId::GOAT;
          c.attr = LIST(10_dam, 7_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setHorseBodyParts(2);
          c.petReaction = "\"Meh-eh-eh!\""_s;
          c.noChase = true;
          c.name = "goat";
      );
  else if (id == "JACKAL")
      return CATTR(
          c.viewId = ViewId::JACKAL;
          c.attr = LIST(15_dam, 10_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(10);
          c.body->setHorseBodyParts(1);
          c.name = "jackal";
      );
  else if (id == "DEER")
      return CATTR(
          c.viewId = ViewId::DEER;
          c.attr = LIST(10_dam, 10_def );
          c.body = Body::nonHumanoid(Body::Size::LARGE);
          c.body->setWeight(400);
          c.body->setHorseBodyParts(2);
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.noChase = true;
          c.name = CreatureName("deer", "deer");
      );
  else if (id == "BOAR")
      return CATTR(
          c.viewId = ViewId::BOAR;
          c.attr = LIST(10_dam, 10_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(200);
          c.body->setHorseBodyParts(5);
          c.noChase = true;
          c.name = "boar";
      );
  else if (id == "FOX")
      return CATTR(
          c.viewId = ViewId::FOX;
          c.attr = LIST(10_dam, 5_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(10);
          c.body->setHorseBodyParts(1);
          c.noChase = true;
          c.name = CreatureName("fox", "foxes");
      );
  else if (id == "CAVE_BEAR")
      return CATTR(
          c.viewId = ViewId::BEAR;
          c.attr = LIST(20_dam, 18_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(250);
          c.body->setHorseBodyParts(8);
          c.body->setIntrinsicAttack(BodyPart::LEG, IntrinsicAttack(ItemType::claws(10)));
          c.permanentEffects[LastingEffect::EXPLORE_CAVES_SKILL] = 1;
          c.name = "cave bear";
      );
  else if (id == "RAT")
      return CATTR(
          c.viewId = ViewId::RAT;
          c.attr = LIST(2_dam, 2_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(1);
          c.body->setHorseBodyParts(1);
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.noChase = true;
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.name = "rat";
      );
  else if (id == "SPIDER")
      return CATTR(
          c.viewId = ViewId::SPIDER;
          c.attr = LIST(9_dam, 13_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(0.3);
          c.body->setBodyParts({{BodyPart::LEG, 8}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(
              ItemType::fangs(1, Effect::Lasting{LastingEffect::POISON})));
          c.name = "spider";
      );
  else if (id == "FLY")
      return CATTR(
          c.viewId = ViewId::FLY;
          c.attr = LIST(2_dam, 12_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(0.1);
          c.body->setBodyParts({{BodyPart::LEG, 6}, {BodyPart::WING, 2}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.courage = 1;
          c.noChase = true;
          c.name = CreatureName("fly", "flies");
      );
  else if (id == "ANT_WORKER")
      return CATTR(
          c.viewId = ViewId::ANT_WORKER;
          c.attr = LIST(16_dam, 16_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(10);
          c.body->setBodyParts({{BodyPart::LEG, 6}, {BodyPart::HEAD, 1}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.name = "giant ant";
      );
  else if (id == "ANT_SOLDIER")
      return CATTR(
          c.viewId = ViewId::ANT_SOLDIER;
          c.attr = LIST(30_dam, 20_def );
          c.permanentEffects[LastingEffect::NAVIGATION_DIGGING_SKILL] = 1;
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(10);
          c.body->setBodyParts({{BodyPart::LEG, 6}, {BodyPart::HEAD, 1}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(6, Effect::Lasting{LastingEffect::POISON})));
          c.name = "giant ant soldier";
      );
  else if (id == "ANT_QUEEN")
      return CATTR(
          c.viewId = ViewId::ANT_QUEEN;
          c.attr = LIST(30_dam, 26_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(10);
          c.body->setBodyParts({{BodyPart::LEG, 6}, {BodyPart::HEAD, 1}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(12, Effect::Lasting{LastingEffect::POISON})));
          c.name = "ant queen";
      );
  else if (id == "SNAKE")
      return CATTR(
          c.viewId = ViewId::SNAKE;
          c.attr = LIST(14_dam, 14_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(2);
          c.body->setBodyParts({{BodyPart::HEAD, 1}, {BodyPart::TORSO, 1}});
          c.body->setDeathSound(none);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(
              ItemType::fangs(1, Effect::Lasting{LastingEffect::POISON})));
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.name = "snake";
      );
  else if (id == "RAVEN")
      return CATTR(
          c.viewId = ViewId::RAVEN;
          c.attr = LIST(2_dam, 12_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(0.5);
          c.body->setBirdBodyParts(1);
          c.body->setDeathSound(none);
          c.noChase = true;
          c.courage = 1;
          c.permanentEffects[LastingEffect::SPEED] = 1;
          c.permanentEffects[LastingEffect::EXPLORE_SKILL] = 1;
          c.name = "raven";
          c.name->setGroup("flock");
      );
  else if (id == "VULTURE")
      return CATTR(
          c.viewId = ViewId::VULTURE;
          c.attr = LIST(2_dam, 12_def );
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(5);
          c.body->setBirdBodyParts(1);
          c.body->setDeathSound(none);
          c.noChase = true;
          c.courage = 1;
          c.name = "vulture";
      );
  else if (id == "WOLF")
      return CATTR(
          c.viewId = ViewId::WOLF;
          c.attr = LIST(18_dam, 11_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(35);
          c.body->setHorseBodyParts(7);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(8)));
          c.body->setIntrinsicAttack(BodyPart::LEG, IntrinsicAttack(ItemType::claws(7)));
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.name = CreatureName("wolf", "wolves");
          c.name->setGroup("pack");
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DOG));
          c.permanentEffects[LastingEffect::EXPLORE_NOCTURNAL_SKILL] = 1;
      );
  else if (id == "WEREWOLF")
      return CATTR(
          c.viewId = ViewId::WEREWOLF;
          c.attr = LIST(20_dam, 7_def );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(8)));
          c.body->setIntrinsicAttack(BodyPart::LEG, IntrinsicAttack(ItemType::claws(7)));
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.permanentEffects[LastingEffect::EXPLORE_NOCTURNAL_SKILL] = 1;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::REGENERATION] = 1;
          c.maxLevelIncrease[ExperienceType::MELEE] = 12;
          c.name = CreatureName("werewolf", "werewolves");
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DOG));
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(8)));
      );
  else if (id == "DOG")
      return CATTR(
          c.viewId = ViewId::DOG;
          c.attr = LIST(18_dam, 7_def );
          c.body = Body::nonHumanoid(Body::Size::MEDIUM);
          c.body->setWeight(25);
          c.body->setHorseBodyParts(2);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(4)));
          c.name = "dog";
          c.name->setGroup("pack");
          c.petReaction = "\"WOOF!\""_s;
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::DOG));
      );
  else if (id == "FIRE_SPHERE")
      return CATTR(
          c.viewId = ViewId::FIRE_SPHERE;
          c.attr = LIST(5_dam, 15_def );
          c.body = Body::nonHumanoid(Body::Material::FIRE, Body::Size::SMALL);
          c.body->setDeathSound(none);
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::LIGHT_SOURCE] = 1;
          c.name = "fire sphere";
      );
  else if (id == "ELEMENTALIST")
      return CATTR(
          c.viewId = ViewId::ELEMENTALIST;
          c.attr = LIST(15_dam, 20_def, 15_spell_dam );
          c.body = Body::humanoid(Body::Size::LARGE);
          c.skills.setValue(SkillId::LABORATORY, 1);
          c.maxLevelIncrease[ExperienceType::SPELL] = 9;
          c.gender = Gender::female;
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::MAGIC_RESISTANCE] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "elementalist";
          c.name->setFirst(nameGenerator->getNext(NameGeneratorId::FIRST_FEMALE));
          c.hatedByEffect = LastingEffect::HATE_HUMANS;
      );
  else if (id == "FIRE_ELEMENTAL")
      return CATTR(
          c.viewId = ViewId::FIRE_ELEMENTAL;
          c.body = Body::nonHumanoid(Body::Material::FIRE, Body::Size::LARGE);
          c.body->setDeathSound(none);
          c.attr = LIST(20_dam, 30_def);
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(ItemType::fists(5, Effect::Fire{})));
          c.permanentEffects[LastingEffect::FIRE_RESISTANT] = 1;
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.name = "fire elemental";
      );
  else if (id == "AIR_ELEMENTAL")
      return CATTR(
          c.viewId = ViewId::AIR_ELEMENTAL;
          c.body = Body::nonHumanoid(Body::Material::SPIRIT, Body::Size::LARGE);
          c.body->setIntrinsicAttack(BodyPart::TORSO, IntrinsicAttack(ItemType::fists(5)));
          c.body->setDeathSound(none);
          c.attr = LIST(25_dam, 35_def );
          c.permanentEffects[LastingEffect::FLYING] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.spells->add(SpellId::CIRCULAR_BLAST);
          c.name = "air elemental";
      );
  else if (id == "EARTH_ELEMENTAL")
      return CATTR(
          c.viewId = ViewId::EARTH_ELEMENTAL;
          c.body = Body::nonHumanoid(Body::Material::ROCK, Body::Size::LARGE);
          c.body->setWeight(500);
          c.body->setHumanoidBodyParts(5);
          c.body->setDeathSound(none);
          c.attr = LIST(30_dam, 25_def );
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.name = "earth elemental";
      );
  else if (id == "WATER_ELEMENTAL")
      return CATTR(
          c.viewId = ViewId::WATER_ELEMENTAL;
          c.body = Body::nonHumanoid(Body::Material::WATER, Body::Size::LARGE);
          c.body->setWeight(300);
          c.body->setHumanoidBodyParts(5);
          c.body->setDeathSound(none);
          c.attr = LIST(40_dam, 15_def );
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SWIMMING_SKILL] = 1;
          c.name = "water elemental";
      );
  else if (id == "ENT")
      return CATTR(
          c.viewId = ViewId::ENT;
          c.body = Body::nonHumanoid(Body::Material::WOOD, Body::Size::HUGE);
          c.body->setHumanoidBodyParts(10);
          c.attr = LIST(35_dam, 25_def);
          c.permanentEffects[LastingEffect::ELF_VISION] = 1;
          c.permanentEffects[LastingEffect::RANGED_RESISTANCE] = 1;
          c.permanentEffects[LastingEffect::SLOWED] = 1;
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "tree spirit";
      );
  else if (id == "ANGEL")
      return CATTR(
          c.viewId = ViewId::ANGEL;
          c.attr = LIST(22_def, 20_spell_dam );
          c.body = Body::nonHumanoid(Body::Material::SPIRIT, Body::Size::LARGE);
          c.chatReactionFriendly = "curses all dungeons"_s;
          c.chatReactionHostile = "\"Die!\""_s;
          c.name = "angel";
      );
  else if (id == "KRAKEN")
      return getKrakenAttributes(ViewId::KRAKEN_HEAD, "kraken");
  else if (id == "BAT")
      return CATTR(
          c.viewId = ViewId::BAT;
          c.body = Body::nonHumanoid(Body::Size::SMALL);
          c.body->setWeight(1);
          c.body->setBirdBodyParts(1);
          c.attr = LIST(3_dam, 16_def);
          c.body->setIntrinsicAttack(BodyPart::HEAD, IntrinsicAttack(ItemType::fangs(3)));
          c.noChase = true;
          c.courage = 1;
          c.permanentEffects[LastingEffect::NIGHT_VISION] = 1;
          c.permanentEffects[LastingEffect::EXPLORE_NOCTURNAL_SKILL] = 1;
          c.permanentEffects[LastingEffect::EXPLORE_CAVES_SKILL] = 1;
          c.name = "bat";
      );
  else if (id == "DEATH")
      return CATTR(
          c.viewId = ViewId::DEATH;
          c.attr = LIST(100_spell_dam, 35_def);
          c.body = Body::humanoidSpirit(Body::Size::LARGE);
          c.chatReactionFriendly = c.chatReactionHostile = "\"IN ORDER TO HAVE A CHANGE OF FORTUNE AT THE LAST MINUTE "
              "YOU HAVE TO TAKE YOUR FORTUNE TO THE LAST MINUTE.\""_s;
          c.name = "Death";
      );
  FATAL << "This is not handled here " << id;
  fail();
}

ControllerFactory getController(CreatureId id, MonsterAIFactory normalFactory) {
  if (id == "KRAKEN")
    return ControllerFactory([=](WCreature c) {
        return makeOwner<KrakenController>(c);
        });
  else
    return Monster::getFactory(normalFactory);
}

PCreature CreatureFactory::get(CreatureId id, TribeId tribe, MonsterAIFactory aiFactory) const {
  ControllerFactory factory = Monster::getFactory(aiFactory);
  if (id == "SPECIAL_BLBN")
    return getSpecial(tribe, false, true, true, false, factory);
  else if (id == "SPECIAL_BLBW")
    return getSpecial(tribe, false, true, true, true, factory);
  else if (id == "SPECIAL_BLGN")
    return getSpecial(tribe, false, true, false, false, factory);
  else if (id == "SPECIAL_BLGW")
    return getSpecial(tribe, false, true, false, true, factory);
  else if (id == "SPECIAL_BMBN")
    return getSpecial(tribe, false, false, true, false, factory);
  else if (id == "SPECIAL_BMBW")
    return getSpecial(tribe, false, false, true, true, factory);
  else if (id == "SPECIAL_BMGN")
    return getSpecial(tribe, false, false, false, false, factory);
  else if (id == "SPECIAL_BMGW")
    return getSpecial(tribe, false, false, false, true, factory);
  else if (id == "SPECIAL_HLBN")
    return getSpecial(tribe, true, true, true, false, factory);
  else if (id == "SPECIAL_HLBW")
    return getSpecial(tribe, true, true, true, true, factory);
  else if (id == "SPECIAL_HLGN")
    return getSpecial(tribe, true, true, false, false, factory);
  else if (id == "SPECIAL_HLGW")
    return getSpecial(tribe, true, true, false, true, factory);
  else if (id == "SPECIAL_HMBN")
    return getSpecial(tribe, true, false, true, false, factory);
  else if (id == "SPECIAL_HMBW")
    return getSpecial(tribe, true, false, true, true, factory);
  else if (id == "SPECIAL_HMGN")
    return getSpecial(tribe, true, false, false, false, factory);
  else if (id == "SPECIAL_HMGW")
    return getSpecial(tribe, true, false, false, true, factory);
  else if (id == "SOKOBAN_BOULDER")
    return getSokobanBoulder(tribe);
  else
    return get(getAttributes(id), tribe, getController(id, aiFactory));
}

PCreature CreatureFactory::getGhost(WCreature creature) const {
  ViewObject viewObject(creature->getViewObject().id(), ViewLayer::CREATURE, "Ghost");
  viewObject.setModifier(ViewObject::Modifier::ILLUSION);
  auto ret = makeOwner<Creature>(viewObject, creature->getTribeId(), getAttributes("LOST_SOUL"));
  ret->setController(Monster::getFactory(MonsterAIFactory::monster()).get(ret.get()));
  return ret;
}

ItemType randomHealing() {
  return ItemType::Potion{Effect::Heal{}};
}

ItemType randomBackup() {
  return Random.choose(
      ItemType(ItemType::Scroll{Effect::Teleport{}}),
      randomHealing());
}

ItemType randomArmor() {
  return Random.choose({ItemType(ItemType::LeatherArmor{}), ItemType(ItemType::ChainArmor{})}, {4, 1});
}

class ItemList {
  public:
  ItemList& maybe(double chance, ItemType id, int num = 1) {
    if (Random.getDouble() <= chance)
      add(id, num);
    return *this;
  }

  ItemList& maybe(double chance, const vector<ItemType>& ids) {
    if (Random.getDouble() <= chance)
      for (ItemType id : ids)
        add(id);
    return *this;
  }

  ItemList& add(ItemType id, int num = 1) {
    for (int i : Range(num))
      ret.push_back(id);
    return *this;
  }

  ItemList& add(vector<ItemType> ids) {
    for (ItemType id : ids)
      ret.push_back(id);
    return *this;
  }

  operator vector<ItemType>() {
    return ret;
  }

  private:
  vector<ItemType> ret;
};

static vector<ItemType> getDefaultInventory(CreatureId id) {
  if (id == "KEEPER_MAGE_F" || id == "KEEPER_MAGE")
    return ItemList()
      .add(ItemType(ItemType::Robe{}));
  else if (id == "KEEPER_KNIGHT_F" || id == "KEEPER_KNIGHT" || id == "KEEPER_KNIGHT_WHITE_F" || id == "KEEPER_KNIGHT_WHITE")
      return ItemList()
        .add(ItemType::LeatherArmor{})
        .add(ItemType::LeatherHelm{})
        .add(ItemType::Sword{});
  else if (id == "CYCLOPS")
      return ItemList()
        .add(ItemType::HeavyClub{})
        .add(ItemType::GoldPiece{}, Random.get(40, 80));
  else if (id == "GREEN_DRAGON")
      return ItemList().add(ItemType::GoldPiece{}, Random.get(60, 100));
  else if (id == "DEMON_DWELLER")
      return ItemList().add(ItemType::GoldPiece{}, Random.get(5, 10));
  else if (id == "RED_DRAGON")
      return ItemList().add(ItemType::GoldPiece{}, Random.get(120, 200));
  else if (id == "DEMON_LORD" || id == "ANGEL")
      return ItemList().add(ItemType(ItemType::Sword{}).setPrefixChance(1));
  else if (id == "ADVENTURER_F" || id == "ADVENTURER")
      return ItemList()
        .add(ItemType::FirstAidKit{}, 3)
        .add(ItemType::Knife{})
        .add(ItemType::Sword{})
        .add(ItemType::LeatherGloves{})
        .add(ItemType::LeatherArmor{})
        .add(ItemType::LeatherHelm{})
        .add(ItemType::GoldPiece{}, Random.get(16, 26));
  else if (id == "ELEMENTALIST")
      return ItemList()
          .add(ItemType::IronStaff{})
          .add(ItemType::Torch{});
  else if (id == "DEATH")
      return ItemList()
        .add(ItemType::Scythe{});
  else if (id == "KOBOLD")
      return ItemList()
        .add(ItemType::Spear{});
  else if (id == "GOBLIN")
      return ItemList()
        .add(ItemType::Club{})
        .maybe(0.3, ItemType::LeatherBoots{});
  else if (id == "WARRIOR")
      return ItemList()
        .add(ItemType::LeatherArmor{})
        .add(ItemType::Club{})
        .add(ItemType::GoldPiece{}, Random.get(2, 5));
  else if (id == "SHAMAN")
      return ItemList()
        .add(ItemType::LeatherArmor{})
        .add(ItemType::Club{})
        .add(ItemType::GoldPiece{}, Random.get(80, 120));
  else if (id == "LIZARDLORD")
      return ItemList().add(ItemType::LeatherArmor{})
        .add(ItemType::Potion{Effect::RegrowBodyPart{}})
        .add(ItemType::GoldPiece{}, Random.get(50, 90));
  else if (id == "LIZARDMAN")
      return ItemList().add(ItemType::LeatherArmor{})
        .add(ItemType::GoldPiece{}, Random.get(2, 4));
  else if (id == "HARPY")
      return ItemList()
        .add(ItemType::Bow{});
  else if (id == "ARCHER")
      return ItemList()
        .add(ItemType::Bow{})
        .add(ItemType::Knife{})
        .add(ItemType::LeatherArmor{})
        .add(ItemType::LeatherBoots{})
        .maybe(0.3, ItemType::Torch{})
        .add(randomHealing())
        .add(ItemType::GoldPiece{}, Random.get(4, 10));
  else if (id == "WITCHMAN")
      return ItemList()
        .add(ItemType::Sword{})
        .add(ItemType::LeatherArmor{})
        .add(ItemType::LeatherBoots{})
        .add(randomHealing())
        .add(ItemType::Potion{Effect::Lasting{LastingEffect::SPEED}}, 4)
        .add(ItemType::GoldPiece{}, Random.get(60, 80));
  else if (id == "PRIEST")
      return ItemList()
        .add(ItemType::IronStaff{})
        .add(ItemType::LeatherBoots{})
        .add(ItemType(ItemType::Robe{}).setPrefixChance(1));
  else if (id == "KNIGHT")
      return ItemList()
        .add(ItemType::Sword{})
        .add(ItemType::ChainArmor{})
        .add(ItemType::LeatherBoots{})
        .maybe(0.3, ItemType::Torch{})
        .add(randomHealing())
        .add(ItemType::GoldPiece{}, Random.get(6, 16));
  else if (id == "MINOTAUR")
      return ItemList()
        .add(ItemType::BattleAxe{});
  else if (id == "DUKE")
      return ItemList()
        .add(ItemType(ItemType::BattleAxe{}).setPrefixChance(1))
        .add(ItemType::ChainArmor{})
        .add(ItemType::IronHelm{})
        .add(ItemType::IronBoots{})
        .add(randomHealing(), 3)
        .maybe(0.3, ItemType::Torch{})
        .add(ItemType::GoldPiece{}, Random.get(140, 200));
  else if (id == "ORC")
      return ItemList()
        .add(ItemType::Club{})
        .add(ItemType::LeatherArmor{});
  else if (id == "OGRE")
      return ItemList().add(ItemType::HeavyClub{});
  else if (id == "BANDIT")
      return ItemList()
        .add(ItemType::Sword{})
        .maybe(0.3, randomBackup())
        .maybe(0.3, ItemType::Torch{})
        .maybe(0.05, ItemType::Bow{});
  else if (id == "DWARF")
      return ItemList()
        .add(Random.choose({ItemType(ItemType::BattleAxe{}), ItemType(ItemType::WarHammer{})}, {1, 1}))
        .maybe(0.6, randomBackup())
        .add(ItemType::ChainArmor{})
        .maybe(0.5, ItemType::IronHelm{})
        .maybe(0.3, ItemType::IronBoots{})
        .maybe(0.3, ItemType::Torch{})
        .add(ItemType::GoldPiece{}, Random.get(2, 6));
  else if (id == "DWARF_BARON")
      return ItemList()
        .add(Random.choose(
            ItemType(ItemType::BattleAxe{}).setPrefixChance(1),
            ItemType(ItemType::WarHammer{}).setPrefixChance(1)))
        .add(randomBackup())
        .add(randomHealing())
        .add(ItemType::ChainArmor{})
        .add(ItemType::IronBoots{})
        .add(ItemType::IronHelm{})
        .maybe(0.3, ItemType::Torch{})
        .add(ItemType::GoldPiece{}, Random.get(80, 120));
  else if (id == "GNOME_CHIEF")
      return ItemList()
        .add(ItemType::Sword{})
        .add(randomBackup());
  else if (id == "VAMPIRE_LORD")
      return ItemList()
        .add(ItemType(ItemType::Robe{}))
        .add(ItemType(ItemType::IronStaff{}));
  else if (id == "DARK_ELF_LORD" || id == "ELF_LORD")
      return ItemList()
        .add(ItemType(ItemType::ElvenSword{}).setPrefixChance(1))
        .add(ItemType::LeatherArmor{})
        .add(ItemType::ElvenBow{})
        .add(ItemType::GoldPiece{}, Random.get(80, 120))
        .add(randomBackup());
  else if (id == "DRIAD")
      return ItemList()
        .add(ItemType::Bow{});
  else if (id == "DARK_ELF_WARRIOR")
      return ItemList()
        .add(ItemType::ElvenSword{})
        .add(ItemType::LeatherArmor{})
        .add(ItemType::GoldPiece{}, Random.get(2, 6))
        .add(randomBackup());
  else if (id == "ELF_ARCHER")
      return ItemList()
        .add(ItemType::ElvenSword{})
        .add(ItemType::LeatherArmor{})
        .add(ItemType::Bow{})
        .add(ItemType::GoldPiece{}, Random.get(2, 6))
        .add(randomBackup());
  else if (id == "WITCH")
      return ItemList()
        .add(ItemType::Knife{})
        .add({
            ItemType::Potion{Effect::Heal{}},
            ItemType::Potion{Effect::Lasting{LastingEffect::SLEEP}},
            ItemType::Potion{Effect::Lasting{LastingEffect::SLOWED}},
            ItemType::Potion{Effect::Lasting{LastingEffect::BLIND}},
            ItemType::Potion{Effect::Lasting{LastingEffect::INVISIBLE}},
            ItemType::Potion{Effect::Lasting{LastingEffect::POISON}},
            ItemType::Potion{Effect::Lasting{LastingEffect::SPEED}}});
  else if (id == "HALLOWEEN_KID")
      return ItemList()
        .add(ItemType::BagOfCandies{})
        .add(ItemType::HalloweenCostume{});
  else
    return {};
}

PCreature CreatureFactory::fromId(CreatureId id, TribeId t) const {
  return fromId(id, t, MonsterAIFactory::monster());
}


PCreature CreatureFactory::fromId(CreatureId id, TribeId t, const MonsterAIFactory& f) const {
  return fromId(id, t, f, {});
}

PCreature CreatureFactory::fromId(CreatureId id, TribeId t, const MonsterAIFactory& factory, const vector<ItemType>& inventory) const {
  auto ret = get(id, t, factory);
  addInventory(ret.get(), inventory);
  addInventory(ret.get(), getDefaultInventory(id));
  return ret;
}

PCreature CreatureFactory::getHumanForTests() {
  auto attributes = CATTR(
      c.viewId = ViewId::KEEPER1;
      c.attr = LIST(12_dam, 12_def, 20_spell_dam );
      c.body = Body::humanoid(Body::Size::LARGE);
      c.name = "wizard";
      c.viewIdUpgrades = LIST(ViewId::KEEPER2, ViewId::KEEPER3, ViewId::KEEPER4);
      c.name->setFirst("keeper");
      c.name->useFullTitle();
      c.skills.setValue(SkillId::LABORATORY, 0.2);
      c.maxLevelIncrease[ExperienceType::MELEE] = 7;
      c.maxLevelIncrease[ExperienceType::SPELL] = 12;
      c.spells->add(SpellId::HEAL_SELF);
  );
  return get(std::move(attributes), TribeId::getMonster(), Monster::getFactory(MonsterAIFactory::idle()));
}
